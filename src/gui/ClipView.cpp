/*
 * ClipView.cpp - implementation of ClipView class
 *
 * Copyright (c) 2004-2014 Tobias Doerffel <tobydox/at/users.sourceforge.net>
 *
 * This file is part of LMMS - https://lmms.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 */

#include "ClipView.h"

#include <QMenu>
#include <QMouseEvent>
#include <QPainter>

#include "AutomationPattern.h"
#include "Clipboard.h"
#include "ColorChooser.h"
#include "ComboBoxModel.h"
#include "DataFile.h"
#include "embed.h"
#include "GuiApplication.h"
#include "SampleTrack.h"
#include "SongEditor.h"
#include "StringPairDrag.h"
#include "TextFloat.h"
#include "TrackContainer.h"
#include "TrackContainerView.h"
#include "TrackView.h"


/*! The width of the resize grip in pixels
 */
const int RESIZE_GRIP_WIDTH = 4;


/*! A pointer for that text bubble used when moving segments, etc.
 *
 * In a number of situations, LMMS displays a floating text bubble
 * beside the cursor as you move or resize elements of a track about.
 * This pointer keeps track of it, as you only ever need one at a time.
 */
TextFloat * ClipView::s_textFloat = NULL;


/*! \brief Create a new trackContentObjectView
 *
 *  Creates a new clip view for the given
 *  clip in the given track view.
 *
 * \param _clip The clip to be displayed
 * \param _tv  The track view that will contain the new object
 */
ClipView::ClipView( Clip * clip,
							TrackView * tv ) :
	selectableObject( tv->getTrackContentWidget() ),
	ModelView( NULL, this ),
	m_clip( clip ),
	m_trackView( tv ),
	m_action( NoAction ),
	m_initialMousePos( QPoint( 0, 0 ) ),
	m_initialMouseGlobalPos( QPoint( 0, 0 ) ),
	m_initialClipPos( TimePos(0) ),
	m_initialClipEnd( TimePos(0) ),
	m_initialOffsets( QVector<TimePos>() ),
	m_hint( NULL ),
	m_mutedColor( 0, 0, 0 ),
	m_mutedBackgroundColor( 0, 0, 0 ),
	m_selectedColor( 0, 0, 0 ),
	m_textColor( 0, 0, 0 ),
	m_textShadowColor( 0, 0, 0 ),
	m_BBPatternBackground( 0, 0, 0 ),
	m_gradient( true ),
	m_mouseHotspotHand( 0, 0 ),
	m_cursorSetYet( false ),
	m_needsUpdate( true )
{
	if( s_textFloat == NULL )
	{
		s_textFloat = new TextFloat;
		s_textFloat->setPixmap( embed::getIconPixmap( "clock" ) );
	}

	setAttribute( Qt::WA_OpaquePaintEvent, true );
	setAttribute( Qt::WA_DeleteOnClose, true );
	setFocusPolicy( Qt::StrongFocus );
	setCursor( QCursor( embed::getIconPixmap( "hand" ), m_mouseHotspotHand.width(), m_mouseHotspotHand.height() ) );
	move( 0, 0 );
	show();

	setFixedHeight( tv->getTrackContentWidget()->height() - 1);
	setAcceptDrops( true );
	setMouseTracking( true );

	connect( m_clip, SIGNAL( lengthChanged() ),
			this, SLOT( updateLength() ) );
	connect( gui->songEditor()->m_editor->zoomingModel(), SIGNAL( dataChanged() ), this, SLOT( updateLength() ) );
	connect( m_clip, SIGNAL( positionChanged() ),
			this, SLOT( updatePosition() ) );
	connect( m_clip, SIGNAL( destroyedClip() ), this, SLOT( close() ) );
	setModel( m_clip );
	connect( m_clip, SIGNAL( trackColorChanged() ), this, SLOT( update() ) );
	connect( m_trackView->getTrackOperationsWidget(), SIGNAL( colorParented() ), this, SLOT( useTrackColor() ) );

	m_trackView->getTrackContentWidget()->addClipView( this );
	updateLength();
	updatePosition();
}




/*! \brief Destroy a trackContentObjectView
 *
 *  Destroys the given clip view.
 *
 */
ClipView::~ClipView()
{
	delete m_hint;
	// we have to give our track-container the focus because otherwise the
	// op-buttons of our track-widgets could become focus and when the user
	// presses space for playing song, just one of these buttons is pressed
	// which results in unwanted effects
	m_trackView->trackContainerView()->setFocus();
}


/*! \brief Update a ClipView
 *
 *  Clip's get drawn only when needed,
 *  and when a Clip is updated,
 *  it needs to be redrawn.
 *
 */
void ClipView::update()
{
	if( !m_cursorSetYet )
	{
		setCursor( QCursor( embed::getIconPixmap( "hand" ), m_mouseHotspotHand.width(), m_mouseHotspotHand.height() ) );
		m_cursorSetYet = true;
	}

	if( fixedClips() )
	{
		updateLength();
	}
	m_needsUpdate = true;
	selectableObject::update();
}



/*! \brief Does this trackContentObjectView have a fixed Clip?
 *
 *  Returns whether the containing trackView has fixed
 *  Clips.
 *
 * \todo What the hell is a Clip here - clip?  And in
 *  what circumstance are they fixed?
 */
bool ClipView::fixedClips()
{
	return m_trackView->trackContainerView()->fixedClips();
}



// qproperty access functions, to be inherited & used by Clipviews
//! \brief CSS theming qproperty access method
QColor ClipView::mutedColor() const
{ return m_mutedColor; }

QColor ClipView::mutedBackgroundColor() const
{ return m_mutedBackgroundColor; }

QColor ClipView::selectedColor() const
{ return m_selectedColor; }

QColor ClipView::textColor() const
{ return m_textColor; }

QColor ClipView::textBackgroundColor() const
{
	return m_textBackgroundColor;
}

QColor ClipView::textShadowColor() const
{ return m_textShadowColor; }

QColor ClipView::BBPatternBackground() const
{ return m_BBPatternBackground; }

bool ClipView::gradient() const
{ return m_gradient; }

//! \brief CSS theming qproperty access method
void ClipView::setMutedColor( const QColor & c )
{ m_mutedColor = QColor( c ); }

void ClipView::setMutedBackgroundColor( const QColor & c )
{ m_mutedBackgroundColor = QColor( c ); }

void ClipView::setSelectedColor( const QColor & c )
{ m_selectedColor = QColor( c ); }

void ClipView::setTextColor( const QColor & c )
{ m_textColor = QColor( c ); }

void ClipView::setTextBackgroundColor( const QColor & c )
{
	m_textBackgroundColor = c;
}

void ClipView::setTextShadowColor( const QColor & c )
{ m_textShadowColor = QColor( c ); }

void ClipView::setBBPatternBackground( const QColor & c )
{ m_BBPatternBackground = QColor( c ); }

void ClipView::setGradient( const bool & b )
{ m_gradient = b; }

void ClipView::setMouseHotspotHand(const QSize & s)
{
	m_mouseHotspotHand = s;
}

// access needsUpdate member variable
bool ClipView::needsUpdate()
{ return m_needsUpdate; }
void ClipView::setNeedsUpdate( bool b )
{ m_needsUpdate = b; }

/*! \brief Close a trackContentObjectView
 *
 *  Closes a clip view by asking the track
 *  view to remove us and then asking the QWidget to close us.
 *
 * \return Boolean state of whether the QWidget was able to close.
 */
bool ClipView::close()
{
	m_trackView->getTrackContentWidget()->removeClipView( this );
	return QWidget::close();
}




/*! \brief Removes a trackContentObjectView from its track view.
 *
 *  Like the close() method, this asks the track view to remove this
 *  clip view.  However, the clip is
 *  scheduled for later deletion rather than closed immediately.
 *
 */
void ClipView::remove()
{
	m_trackView->getTrack()->addJournalCheckPoint();

	// delete ourself
	close();
	m_clip->deleteLater();
}




/*! \brief Updates a trackContentObjectView's length
 *
 *  If this clip view has a fixed Clip, then we must
 *  keep the width of our parent.  Otherwise, calculate our width from
 *  the clip's length in pixels adding in the border.
 *
 */
void ClipView::updateLength()
{
	if( fixedClips() )
	{
		setFixedWidth( parentWidget()->width() );
	}
	else
	{
		setFixedWidth(
		static_cast<int>( m_clip->length() * pixelsPerBar() /
					TimePos::ticksPerBar() ) + 1 /*+
						Clip_BORDER_WIDTH * 2-1*/ );
	}
	m_trackView->trackContainerView()->update();
}




/*! \brief Updates a trackContentObjectView's position.
 *
 *  Ask our track view to change our position.  Then make sure that the
 *  track view is updated in case this position has changed the track
 *  view's length.
 *
 */
void ClipView::updatePosition()
{
	m_trackView->getTrackContentWidget()->changePosition();
	// moving a Clip can result in change of song-length etc.,
	// therefore we update the track-container
	m_trackView->trackContainerView()->update();
}




void ClipView::changeClipColor()
{
	// Get a color from the user
	QColor new_color = ColorChooser( this ).withPalette( ColorChooser::Palette::Track )->getColor( m_clip->color() );
	if( ! new_color.isValid() )
	{ return; }

	// Use that color
	m_clip->setColor( new_color );
	m_clip->useCustomClipColor( true );
	update();
}



void ClipView::useTrackColor()
{
	m_clip->useCustomClipColor( false );
	update();
}





/*! \brief Change the trackContentObjectView's display when something
 *  being dragged enters it.
 *
 *  We need to notify Qt to change our display if something being
 *  dragged has entered our 'airspace'.
 *
 * \param dee The QDragEnterEvent to watch.
 */
void ClipView::dragEnterEvent( QDragEnterEvent * dee )
{
	TrackContentWidget * tcw = getTrackView()->getTrackContentWidget();
	TimePos clipPos = TimePos( m_clip->startPosition() );

	if( tcw->canPasteSelection( clipPos, dee ) == false )
	{
		dee->ignore();
	}
	else
	{
		StringPairDrag::processDragEnterEvent( dee, "clip_" +
					QString::number( m_clip->getTrack()->type() ) );
	}
}




/*! \brief Handle something being dropped on this trackContentObjectView.
 *
 *  When something has been dropped on this trackContentObjectView, and
 *  it's a clip, then use an instance of our dataFile reader
 *  to take the xml of the clip and turn it into something
 *  we can write over our current state.
 *
 * \param de The QDropEvent to handle.
 */
void ClipView::dropEvent( QDropEvent * de )
{
	QString type = StringPairDrag::decodeKey( de );
	QString value = StringPairDrag::decodeValue( de );

	// Track must be the same type to paste into
	if( type != ( "clip_" + QString::number( m_clip->getTrack()->type() ) ) )
	{
		return;
	}

	// Defer to rubberband paste if we're in that mode
	if( m_trackView->trackContainerView()->allowRubberband() == true )
	{
		TrackContentWidget * tcw = getTrackView()->getTrackContentWidget();
		TimePos clipPos = TimePos( m_clip->startPosition() );

		if( tcw->pasteSelection( clipPos, de ) == true )
		{
			de->accept();
		}
		return;
	}

	// Don't allow pasting a clip into itself.
	QObject* qwSource = de->source();
	if( qwSource != NULL &&
	    dynamic_cast<ClipView *>( qwSource ) == this )
	{
		return;
	}

	// Copy state into existing clip
	DataFile dataFile( value.toUtf8() );
	TimePos pos = m_clip->startPosition();
	QDomElement clips = dataFile.content().firstChildElement( "clips" );
	m_clip->restoreState( clips.firstChildElement().firstChildElement() );
	m_clip->movePosition( pos );
	AutomationPattern::resolveAllIDs();
	de->accept();
}




/*! \brief Handle a dragged selection leaving our 'airspace'.
 *
 * \param e The QEvent to watch.
 */
void ClipView::leaveEvent( QEvent * e )
{
	if( cursor().shape() != Qt::BitmapCursor )
	{
		setCursor( QCursor( embed::getIconPixmap( "hand" ), m_mouseHotspotHand.width(), m_mouseHotspotHand.height() ) );
	}
	if( e != NULL )
	{
		QWidget::leaveEvent( e );
	}
}

/*! \brief Create a DataFile suitable for copying multiple trackContentObjects.
 *
 *	trackContentObjects in the vector are written to the "clips" node in the
 *  DataFile.  The trackContentObjectView's initial mouse position is written
 *  to the "initialMouseX" node in the DataFile.  When dropped on a track,
 *  this is used to create copies of the Clips.
 *
 * \param clips The trackContectObjects to save in a DataFile
 */
DataFile ClipView::createClipDataFiles(
    				const QVector<ClipView *> & clipViews) const
{
	Track * t = m_trackView->getTrack();
	TrackContainer * tc = t->trackContainer();
	DataFile dataFile( DataFile::DragNDropData );
	QDomElement clipParent = dataFile.createElement( "clips" );

	typedef QVector<ClipView *> clipViewVector;
	for( clipViewVector::const_iterator it = clipViews.begin();
			it != clipViews.end(); ++it )
	{
		// Insert into the dom under the "clips" element
		Track* clipTrack = ( *it )->m_trackView->getTrack();
		int trackIndex = tc->tracks().indexOf( clipTrack );
		QDomElement clipElement = dataFile.createElement( "clip" );
		clipElement.setAttribute( "trackIndex", trackIndex );
		clipElement.setAttribute( "trackType", clipTrack->type() );
		clipElement.setAttribute( "trackName", clipTrack->name() );
		( *it )->m_clip->saveState( dataFile, clipElement );
		clipParent.appendChild( clipElement );
	}

	dataFile.content().appendChild( clipParent );

	// Add extra metadata needed for calculations later
	int initialTrackIndex = tc->tracks().indexOf( t );
	if( initialTrackIndex < 0 )
	{
		printf("Failed to find selected track in the TrackContainer.\n");
		return dataFile;
	}
	QDomElement metadata = dataFile.createElement( "copyMetadata" );
	// initialTrackIndex is the index of the track that was touched
	metadata.setAttribute( "initialTrackIndex", initialTrackIndex );
	metadata.setAttribute( "trackContainerId", tc->id() );
	// grabbedClipPos is the pos of the bar containing the Clip we grabbed
	metadata.setAttribute( "grabbedClipPos", m_clip->startPosition() );

	dataFile.content().appendChild( metadata );

	return dataFile;
}

void ClipView::paintTextLabel(QString const & text, QPainter & painter)
{
	if (text.trimmed() == "")
	{
		return;
	}

	painter.setRenderHint( QPainter::TextAntialiasing );

	QFont labelFont = this->font();
	labelFont.setHintingPreference( QFont::PreferFullHinting );
	painter.setFont( labelFont );

	const int textTop = Clip_BORDER_WIDTH + 1;
	const int textLeft = Clip_BORDER_WIDTH + 3;

	QFontMetrics fontMetrics(labelFont);
	QString elidedPatternName = fontMetrics.elidedText(text, Qt::ElideMiddle, width() - 2 * textLeft);

	if (elidedPatternName.length() < 2)
	{
		elidedPatternName = text.trimmed();
	}

	painter.fillRect(QRect(0, 0, width(), fontMetrics.height() + 2 * textTop), textBackgroundColor());

	int const finalTextTop = textTop + fontMetrics.ascent();
	painter.setPen(textShadowColor());
	painter.drawText( textLeft + 1, finalTextTop + 1, elidedPatternName );
	painter.setPen( textColor() );
	painter.drawText( textLeft, finalTextTop, elidedPatternName );
}

/*! \brief Handle a mouse press on this trackContentObjectView.
 *
 *  Handles the various ways in which a trackContentObjectView can be
 *  used with a click of a mouse button.
 *
 *  * If our container supports rubber band selection then handle
 *    selection events.
 *  * or if shift-left button, add this object to the selection
 *  * or if ctrl-left button, start a drag-copy event
 *  * or if just plain left button, resize if we're resizeable
 *  * or if ctrl-middle button, mute the clip
 *  * or if middle button, maybe delete the clip.
 *
 * \param me The QMouseEvent to handle.
 */
void ClipView::mousePressEvent( QMouseEvent * me )
{
	// Right now, active is only used on right/mid clicks actions, so we use a ternary operator
	// to avoid the overhead of calling getClickedClips when it's not used
	auto active = me->button() == Qt::LeftButton
		? QVector<ClipView *>()
		: getClickedClips();

	setInitialPos( me->pos() );
	setInitialOffsets();
	if( !fixedClips() && me->button() == Qt::LeftButton )
	{
		if( me->modifiers() & Qt::ControlModifier )
		{
			if( isSelected() )
			{
				m_action = CopySelection;
			}
			else
			{
				m_action = ToggleSelected;
			}
		}
		else if( !me->modifiers()
			|| (me->modifiers() & Qt::AltModifier)
			|| (me->modifiers() & Qt::ShiftModifier) )
		{
			if( isSelected() )
			{
				m_action = MoveSelection;
			}
			else
			{
				gui->songEditor()->m_editor->selectAllClips( false );
				m_clip->addJournalCheckPoint();

				// move or resize
				m_clip->setJournalling( false );

				setInitialPos( me->pos() );
				setInitialOffsets();

				SampleClip * sClip = dynamic_cast<SampleClip*>( m_clip );
				if( me->x() < RESIZE_GRIP_WIDTH && sClip
						&& !m_clip->getAutoResize() )
				{
					m_action = ResizeLeft;
					setCursor( Qt::SizeHorCursor );
				}
				else if( m_clip->getAutoResize() || me->x() < width() - RESIZE_GRIP_WIDTH )
				{
					m_action = Move;
					setCursor( Qt::SizeAllCursor );
				}
				else
				{
					m_action = Resize;
					setCursor( Qt::SizeHorCursor );
				}

				if( m_action == Move )
				{
					s_textFloat->setTitle( tr( "Current position" ) );
					s_textFloat->setText( QString( "%1:%2" ).
						arg( m_clip->startPosition().getBar() + 1 ).
						arg( m_clip->startPosition().getTicks() %
								TimePos::ticksPerBar() ) );
				}
				else if( m_action == Resize || m_action == ResizeLeft )
				{
					s_textFloat->setTitle( tr( "Current length" ) );
					s_textFloat->setText( tr( "%1:%2 (%3:%4 to %5:%6)" ).
							arg( m_clip->length().getBar() ).
							arg( m_clip->length().getTicks() %
									TimePos::ticksPerBar() ).
							arg( m_clip->startPosition().getBar() + 1 ).
							arg( m_clip->startPosition().getTicks() %
									TimePos::ticksPerBar() ).
							arg( m_clip->endPosition().getBar() + 1 ).
							arg( m_clip->endPosition().getTicks() %
									TimePos::ticksPerBar() ) );
				}
				// s_textFloat->reparent( this );
				// setup text-float as if Clip was already moved/resized
				s_textFloat->moveGlobal( this, QPoint( width() + 2, height() + 2) );
				s_textFloat->show();
			}

			delete m_hint;
			QString hint = m_action == Move || m_action == MoveSelection
						? tr( "Press <%1> and drag to make a copy." )
						: tr( "Press <%1> for free resizing." );
			m_hint = TextFloat::displayMessage( tr( "Hint" ), hint.arg(UI_CTRL_KEY),
					embed::getIconPixmap( "hint" ), 0 );
		}
	}
	else if( me->button() == Qt::RightButton )
	{
		if( me->modifiers() & Qt::ControlModifier )
		{
			toggleMute( active );
		}
		else if( me->modifiers() & Qt::ShiftModifier && !fixedClips() )
		{
			remove( active );
		}
	}
	else if( me->button() == Qt::MidButton )
	{
		if( me->modifiers() & Qt::ControlModifier )
		{
			toggleMute( active );
		}
		else if( !fixedClips() )
		{
			remove( active );
		}
	}
}




/*! \brief Handle a mouse movement (drag) on this trackContentObjectView.
 *
 *  Handles the various ways in which a trackContentObjectView can be
 *  used with a mouse drag.
 *
 *  * If in move mode, move ourselves in the track,
 *  * or if in move-selection mode, move the entire selection,
 *  * or if in resize mode, resize ourselves,
 *  * otherwise ???
 *
 * \param me The QMouseEvent to handle.
 * \todo what does the final else case do here?
 */
void ClipView::mouseMoveEvent( QMouseEvent * me )
{
	if( m_action == CopySelection || m_action == ToggleSelected )
	{
		if( mouseMovedDistance( me, 2 ) == true )
		{
			QVector<ClipView *> clipViews;
			if( m_action == CopySelection )
			{
				// Collect all selected Clips
				QVector<selectableObject *> so =
					m_trackView->trackContainerView()->selectedObjects();
				for( auto it = so.begin(); it != so.end(); ++it )
				{
					ClipView * clipv =
						dynamic_cast<ClipView *>( *it );
					if( clipv != NULL )
					{
						clipViews.push_back( clipv );
					}
				}
			}
			else
			{
				gui->songEditor()->m_editor->selectAllClips( false );
				clipViews.push_back( this );
			}
			// Clear the action here because mouseReleaseEvent will not get
			// triggered once we go into drag.
			m_action = NoAction;

			// Write the Clips to the DataFile for copying
			DataFile dataFile = createClipDataFiles( clipViews );

			// TODO -- thumbnail for all selected
			QPixmap thumbnail = grab().scaled(
				128, 128,
				Qt::KeepAspectRatio,
				Qt::SmoothTransformation );
			new StringPairDrag( QString( "clip_%1" ).arg(
								m_clip->getTrack()->type() ),
								dataFile.toString(), thumbnail, this );
		}
	}

	if( me->modifiers() & Qt::ControlModifier )
	{
		delete m_hint;
		m_hint = NULL;
	}

	const float ppb = m_trackView->trackContainerView()->pixelsPerBar();
	if( m_action == Move )
	{
		TimePos newPos = draggedClipPos( me );

		m_clip->movePosition(newPos);
		newPos = m_clip->startPosition(); // Get the real position the Clip was dragged to for the label
		m_trackView->getTrackContentWidget()->changePosition();
		s_textFloat->setText( QString( "%1:%2" ).
				arg( newPos.getBar() + 1 ).
				arg( newPos.getTicks() %
						TimePos::ticksPerBar() ) );
		s_textFloat->moveGlobal( this, QPoint( width() + 2, height() + 2 ) );
	}
	else if( m_action == MoveSelection )
	{
		// 1: Find the position we want to move the grabbed Clip to
		TimePos newPos = draggedClipPos( me );

		// 2: Handle moving the other selected Clips the same distance
		QVector<selectableObject *> so =
			m_trackView->trackContainerView()->selectedObjects();
		QVector<Clip *> clips; // List of selected clips
		int leftmost = 0; // Leftmost clip's offset from grabbed clip
		// Populate clips, find leftmost
		for( QVector<selectableObject *>::iterator it = so.begin();
							it != so.end(); ++it )
		{
			ClipView * clipv =
				dynamic_cast<ClipView *>( *it );
			if( clipv == NULL ) { continue; }
			clips.push_back( clipv->m_clip );
			int index = std::distance( so.begin(), it );
			leftmost = std::min(leftmost, m_initialOffsets[index].getTicks());
		}
		// Make sure the leftmost clip doesn't get moved to a negative position
		if ( newPos.getTicks() + leftmost < 0 ) { newPos = -leftmost; }

		for( QVector<Clip *>::iterator it = clips.begin();
							it != clips.end(); ++it )
		{
			int index = std::distance( clips.begin(), it );
			( *it )->movePosition( newPos + m_initialOffsets[index] );
		}
	}
	else if( m_action == Resize || m_action == ResizeLeft )
	{
		// If the user is holding alt, or pressed ctrl after beginning the drag, don't quantize
		const bool unquantized = (me->modifiers() & Qt::ControlModifier) || (me->modifiers() & Qt::AltModifier);
		const float snapSize = gui->songEditor()->m_editor->getSnapSize();
		// Length in ticks of one snap increment
		const TimePos snapLength = TimePos( (int)(snapSize * TimePos::ticksPerBar()) );

		if( m_action == Resize )
		{
			// The clip's new length
			TimePos l = static_cast<int>( me->x() * TimePos::ticksPerBar() / ppb );

			if ( unquantized )
			{	// We want to preserve this adjusted offset,
				// even if the user switches to snapping later
				setInitialPos( m_initialMousePos );
				// Don't resize to less than 1 tick
				m_clip->changeLength( qMax<int>( 1, l ) );
			}
			else if ( me->modifiers() & Qt::ShiftModifier )
			{	// If shift is held, quantize clip's end position
				TimePos end = TimePos( m_initialClipPos + l ).quantize( snapSize );
				// The end position has to be after the clip's start
				TimePos min = m_initialClipPos.quantize( snapSize );
				if ( min <= m_initialClipPos ) min += snapLength;
				m_clip->changeLength( qMax<int>(min - m_initialClipPos, end - m_initialClipPos) );
			}
			else
			{	// Otherwise, resize in fixed increments
				TimePos initialLength = m_initialClipEnd - m_initialClipPos;
				TimePos offset = TimePos( l - initialLength ).quantize( snapSize );
				// Don't resize to less than 1 tick
				TimePos min = TimePos( initialLength % snapLength );
				if (min < 1) min += snapLength;
				m_clip->changeLength( qMax<int>( min, initialLength + offset) );
			}
		}
		else
		{
			SampleClip * sClip = dynamic_cast<SampleClip*>( m_clip );
			if( sClip )
			{
				const int x = mapToParent( me->pos() ).x() - m_initialMousePos.x();

				TimePos t = qMax( 0, (int)
									m_trackView->trackContainerView()->currentPosition() +
									static_cast<int>( x * TimePos::ticksPerBar() / ppb ) );

				if( unquantized )
				{	// We want to preserve this adjusted offset,
					// even if the user switches to snapping later
					setInitialPos( m_initialMousePos );
					//Don't resize to less than 1 tick
					t = qMin<int>( m_initialClipEnd - 1, t);
				}
				else if( me->modifiers() & Qt::ShiftModifier )
				{	// If shift is held, quantize clip's start position
					// Don't let the start position move past the end position
					TimePos max = m_initialClipEnd.quantize( snapSize );
					if ( max >= m_initialClipEnd ) max -= snapLength;
					t = qMin<int>( max, t.quantize( snapSize ) );
				}
				else
				{	// Otherwise, resize in fixed increments
					// Don't resize to less than 1 tick
					TimePos initialLength = m_initialClipEnd - m_initialClipPos;
					TimePos minLength = TimePos( initialLength % snapLength );
					if (minLength < 1) minLength += snapLength;
					TimePos offset = TimePos(t - m_initialClipPos).quantize( snapSize );
					t = qMin<int>( m_initialClipEnd - minLength, m_initialClipPos + offset );
				}

				TimePos oldPos = m_clip->startPosition();
				if( m_clip->length() + ( oldPos - t ) >= 1 )
				{
					m_clip->movePosition( t );
					m_clip->changeLength( m_clip->length() + ( oldPos - t ) );
					sClip->setStartTimeOffset( sClip->startTimeOffset() + ( oldPos - t ) );
				}
			}
		}
		s_textFloat->setText( tr( "%1:%2 (%3:%4 to %5:%6)" ).
				arg( m_clip->length().getBar() ).
				arg( m_clip->length().getTicks() %
						TimePos::ticksPerBar() ).
				arg( m_clip->startPosition().getBar() + 1 ).
				arg( m_clip->startPosition().getTicks() %
						TimePos::ticksPerBar() ).
				arg( m_clip->endPosition().getBar() + 1 ).
				arg( m_clip->endPosition().getTicks() %
						TimePos::ticksPerBar() ) );
		s_textFloat->moveGlobal( this, QPoint( width() + 2, height() + 2) );
	}
	else
	{
		SampleClip * sClip = dynamic_cast<SampleClip*>( m_clip );
		if( ( me->x() > width() - RESIZE_GRIP_WIDTH && !me->buttons() && !m_clip->getAutoResize() )
		||  ( me->x() < RESIZE_GRIP_WIDTH && !me->buttons() && sClip && !m_clip->getAutoResize() ) )
		{
			setCursor( Qt::SizeHorCursor );
		}
		else
		{
			leaveEvent( NULL );
		}
	}
}




/*! \brief Handle a mouse release on this trackContentObjectView.
 *
 *  If we're in move or resize mode, journal the change as appropriate.
 *  Then tidy up.
 *
 * \param me The QMouseEvent to handle.
 */
void ClipView::mouseReleaseEvent( QMouseEvent * me )
{
	// If the CopySelection was chosen as the action due to mouse movement,
	// it will have been cleared.  At this point Toggle is the desired action.
	// An active StringPairDrag will prevent this method from being called,
	// so a real CopySelection would not have occurred.
	if( m_action == CopySelection ||
	    ( m_action == ToggleSelected && mouseMovedDistance( me, 2 ) == false ) )
	{
		setSelected( !isSelected() );
	}

	if( m_action == Move || m_action == Resize || m_action == ResizeLeft )
	{
		// TODO: Fix m_clip->setJournalling() consistency
		m_clip->setJournalling( true );
	}
	m_action = NoAction;
	delete m_hint;
	m_hint = NULL;
	s_textFloat->hide();
	leaveEvent( NULL );
	selectableObject::mouseReleaseEvent( me );
}




/*! \brief Set up the context menu for this trackContentObjectView.
 *
 *  Set up the various context menu events that can apply to a
 *  clip view.
 *
 * \param cme The QContextMenuEvent to add the actions to.
 */
void ClipView::contextMenuEvent( QContextMenuEvent * cme )
{
	// Depending on whether we right-clicked a selection or an individual Clip we will have
	// different labels for the actions.
	bool individualClip = getClickedClips().size() <= 1;

	if( cme->modifiers() )
	{
		return;
	}

	QMenu contextMenu( this );

	if( fixedClips() == false )
	{
		contextMenu.addAction(
			embed::getIconPixmap( "cancel" ),
			individualClip
				? tr("Delete (middle mousebutton)")
				: tr("Delete selection (middle mousebutton)"),
			[this](){ contextMenuAction( Remove ); } );

		contextMenu.addSeparator();

		contextMenu.addAction(
			embed::getIconPixmap( "edit_cut" ),
			individualClip
				? tr("Cut")
				: tr("Cut selection"),
			[this](){ contextMenuAction( Cut ); } );
	}

	contextMenu.addAction(
		embed::getIconPixmap( "edit_copy" ),
		individualClip
			? tr("Copy")
			: tr("Copy selection"),
		[this](){ contextMenuAction( Copy ); } );

	contextMenu.addAction(
		embed::getIconPixmap( "edit_paste" ),
		tr( "Paste" ),
		[this](){ contextMenuAction( Paste ); } );

	contextMenu.addSeparator();

	contextMenu.addAction(
		embed::getIconPixmap( "muted" ),
		(individualClip
			? tr("Mute/unmute (<%1> + middle click)")
			: tr("Mute/unmute selection (<%1> + middle click)")).arg(UI_CTRL_KEY),
		[this](){ contextMenuAction( Mute ); } );

	contextMenu.addSeparator();

	contextMenu.addAction( embed::getIconPixmap( "colorize" ),
			tr( "Set clip color" ), this, SLOT( changeClipColor() ) );
	contextMenu.addAction( embed::getIconPixmap( "colorize" ),
			tr( "Use track color" ), this, SLOT( useTrackColor() ) );

	constructContextMenu( &contextMenu );

	contextMenu.exec( QCursor::pos() );
}

// This method processes the actions from the context menu of the Clip View.
void ClipView::contextMenuAction( ContextMenuAction action )
{
	QVector<ClipView *> active = getClickedClips();
	// active will be later used for the remove, copy, cut or toggleMute methods

	switch( action )
	{
		case Remove:
			remove( active );
			break;
		case Cut:
			cut( active );
			break;
		case Copy:
			copy( active );
			break;
		case Paste:
			paste();
			break;
		case Mute:
			toggleMute( active );
			break;
	}
}

QVector<ClipView *> ClipView::getClickedClips()
{
	// Get a list of selected selectableObjects
	QVector<selectableObject *> sos = gui->songEditor()->m_editor->selectedObjects();

	// Convert to a list of selected ClipVs
	QVector<ClipView *> selection;
	selection.reserve( sos.size() );
	for( auto so: sos )
	{
		ClipView *clipv = dynamic_cast<ClipView *> ( so );
		if( clipv != nullptr )
		{
			selection.append( clipv );
		}
	}

	// If we clicked part of the selection, affect all selected clips. Otherwise affect the clip we clicked
	return selection.contains(this)
		? selection
		: QVector<ClipView *>( 1, this );
}

void ClipView::remove( QVector<ClipView *> clipvs )
{
	for( auto clipv: clipvs )
	{
		// No need to check if it's nullptr because we check when building the QVector
		clipv->remove();
	}
}

void ClipView::copy( QVector<ClipView *> clipvs )
{
	// For copyStringPair()
	using namespace Clipboard;

	// Write the Clips to a DataFile for copying
	DataFile dataFile = createClipDataFiles( clipvs );

	// Copy the Clip type as a key and the Clip data file to the clipboard
	copyStringPair( QString( "clip_%1" ).arg( m_clip->getTrack()->type() ),
		dataFile.toString() );
}

void ClipView::cut( QVector<ClipView *> clipvs )
{
	// Copy the selected Clips
	copy( clipvs );

	// Now that the Clips are copied we can delete them, since we are cutting
	remove( clipvs );
}

void ClipView::paste()
{
	// For getMimeData()
	using namespace Clipboard;

	// If possible, paste the selection on the TimePos of the selected Track and remove it
	TimePos clipPos = TimePos( m_clip->startPosition() );

	TrackContentWidget *tcw = getTrackView()->getTrackContentWidget();

	if( tcw->pasteSelection( clipPos, getMimeData() ) )
	{
		// If we succeed on the paste we delete the Clip we pasted on
		remove();
	}
}

void ClipView::toggleMute( QVector<ClipView *> clipvs )
{
	for( auto clipv: clipvs )
	{
		// No need to check for nullptr because we check while building the clipvs QVector
		clipv->getClip()->toggleMute();
	}
}




/*! \brief How many pixels a bar takes for this trackContentObjectView.
 *
 * \return the number of pixels per bar.
 */
float ClipView::pixelsPerBar()
{
	return m_trackView->trackContainerView()->pixelsPerBar();
}


/*! \brief Save the offsets between all selected tracks and a clicked track */
void ClipView::setInitialOffsets()
{
	QVector<selectableObject *> so = m_trackView->trackContainerView()->selectedObjects();
	QVector<TimePos> offsets;
	for( QVector<selectableObject *>::iterator it = so.begin();
						it != so.end(); ++it )
	{
		ClipView * clipv =
			dynamic_cast<ClipView *>( *it );
		if( clipv == NULL )
		{
			continue;
		}
		offsets.push_back( clipv->m_clip->startPosition() - m_initialClipPos );
	}

	m_initialOffsets = offsets;
}




/*! \brief Detect whether the mouse moved more than n pixels on screen.
 *
 * \param _me The QMouseEvent.
 * \param distance The threshold distance that the mouse has moved to return true.
 */
bool ClipView::mouseMovedDistance( QMouseEvent * me, int distance )
{
	QPoint dPos = mapToGlobal( me->pos() ) - m_initialMouseGlobalPos;
	const int pixelsMoved = dPos.manhattanLength();
	return ( pixelsMoved > distance || pixelsMoved < -distance );
}



/*! \brief Calculate the new position of a dragged Clip from a mouse event
 *
 *
 * \param me The QMouseEvent
 */
TimePos ClipView::draggedClipPos( QMouseEvent * me )
{
	//Pixels per bar
	const float ppb = m_trackView->trackContainerView()->pixelsPerBar();
	// The pixel distance that the mouse has moved
	const int mouseOff = mapToGlobal(me->pos()).x() - m_initialMouseGlobalPos.x();
	TimePos newPos = m_initialClipPos + mouseOff * TimePos::ticksPerBar() / ppb;
	TimePos offset = newPos - m_initialClipPos;
	// If the user is holding alt, or pressed ctrl after beginning the drag, don't quantize
	if (    me->button() != Qt::NoButton
		|| (me->modifiers() & Qt::ControlModifier)
		|| (me->modifiers() & Qt::AltModifier)    )
	{
		// We want to preserve this adjusted offset,
		// even if the user switches to snapping
		setInitialPos( m_initialMousePos );
	}
	else if ( me->modifiers() & Qt::ShiftModifier )
	{	// If shift is held, quantize position (Default in 1.2.0 and earlier)
		// or end position, whichever is closest to the actual position
		TimePos startQ = newPos.quantize( gui->songEditor()->m_editor->getSnapSize() );
		// Find start position that gives snapped clip end position
		TimePos endQ = ( newPos + m_clip->length() );
		endQ = endQ.quantize( gui->songEditor()->m_editor->getSnapSize() );
		endQ = endQ - m_clip->length();
		// Select the position closest to actual position
		if ( abs(newPos - startQ) < abs(newPos - endQ) ) newPos = startQ;
		else newPos = endQ;
	}
	else
	{	// Otherwise, quantize moved distance (preserves user offsets)
		newPos = m_initialClipPos + offset.quantize( gui->songEditor()->m_editor->getSnapSize() );
	}
	return newPos;
}


// Return the color that the Clip's background should be
QColor ClipView::getColorForDisplay( QColor defaultColor )
{
	// Get the pure Clip color
	auto clipColor = m_clip->hasColor()
					? m_clip->usesCustomClipColor()
						? m_clip->color()
						: m_clip->getTrack()->color()
					: defaultColor;

	// Set variables
	QColor c, mutedCustomColor;
	bool muted = m_clip->getTrack()->isMuted() || m_clip->isMuted();
	mutedCustomColor = clipColor;
	mutedCustomColor.setHsv( mutedCustomColor.hsvHue(), mutedCustomColor.hsvSaturation() / 4, mutedCustomColor.value() );

	// Change the pure color by state: selected, muted, colored, normal
	if( isSelected() )
	{
		c = m_clip->hasColor()
			? ( muted
				? mutedCustomColor.darker( 350 )
				: clipColor.darker( 150 ) )
			: selectedColor();
	}
	else
	{
		if( muted )
		{
			c = m_clip->hasColor()
				? mutedCustomColor.darker( 250 )
				: mutedBackgroundColor();
		}
		else
		{
			c = clipColor;
		}
	}

	// Return color to caller
	return c;
}
