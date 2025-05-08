// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import android.content.res.Resources;
import android.view.MotionEvent;
import android.view.View;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.components.browser_ui.widget.tile.TileView;
import org.chromium.ui.util.RunnableTimer;

import java.util.ArrayList;
import java.util.List;

/**
 * UI logic for dragging a "from" tile to a "to" tile's location: Handles touch events, updates the
 * the state machine. Reusable across drag sessions for a {@link MostVisitedTilesLayout} instance.
 * Uses {@link TileDragSession} to store per-session states.
 */
@NullMarked
class TileDragDelegateImpl implements TileGroup.TileDragDelegate, TileDragSession.Delegate {

    // Tile drag dynamics are represented by a state machine. Here are its states.
    @IntDef({
        DragPhase.NONE,
        DragPhase.PREPARE,
        DragPhase.START,
        DragPhase.DOMINATE,
    })
    public @interface DragPhase {
        // NONE: No drag. ACTION_DOWN => :=PREPARE (i.e., enter the PREPARE phase).
        int NONE = 0;

        // PREPARE: No drag. Default touch handling triggers ACTION_UP => tile click; ACTION_MOVE
        // (after small drag) => scroll. These default interactions trigger ACTION_CANCEL => :=NONE.
        // If PREPARE persists longer than "start duration" then :=START. TileDragHandlerDelegate
        // is passed here.
        int PREPARE = 1;

        // START: Tile drag is live: ACTION_MOVE => move "from" tile; ACTION_UP => cancel drag
        // :=NONE. If drag displacement exceeds "dominate threshold" then :=DOMINATE, and call
        // TileDragHandlerDelegate.onDragDominate().
        int START = 2;

        // DOMINATE: Tile drag is live: ACTION_MOVE => move "from" and background tiles; ACTION_UP
        // => finalize, which *may* call TileDragHandlerDelegate.onDragAccept(), and then :=NONE.
        int DOMINATE = 3;

        int NUM_ENTRIES = 4;
    }

    // "Start duration": Delay (in ms) for triggering PREPARE -> START change.
    private static final long START_DURATION_MS = 300;

    // Relative "dominate threshold": Multiplied by tile width to get "dominate threshold".
    private static final float DOMINATE_TRESHOLD_RATIO = 0.4f;

    // Parent container for dragged tiles.
    private final MostVisitedTilesLayout mMvTilesLayout;

    private final float mTileWidthPx;

    // Squared "dominate threshold": During drag, if the ACTION_MOVE position's (Euclidean) distance
    // to the ACTION_DOWN distance exceeds "dominate threshold" then START -> DOMINATE change
    // triggers.
    private final float mDominateThresholdPxSquared;

    // Timer for PREPARE -> START change.
    private final RunnableTimer mTimer = new RunnableTimer();

    // Current UI phase.
    private @DragPhase int mPhase;

    // Ephemeral drag states: Null in EMPTY; assigned in PREPARE; active in {START, DOMINATE}.
    private @Nullable TileDragSession mTileDragSession;

    // Runnable to cancel tile movement that might not have completed yet, due to animation.
    private @Nullable Runnable mPendingChangeCanceller;

    public TileDragDelegateImpl(MostVisitedTilesLayout mvTilesLayout) {
        mMvTilesLayout = mvTilesLayout;
        Resources res = mMvTilesLayout.getResources();
        mTileWidthPx = res.getDimensionPixelSize(R.dimen.tile_view_width);
        float mDominateThresholdPx = DOMINATE_TRESHOLD_RATIO * mTileWidthPx;
        mDominateThresholdPxSquared = mDominateThresholdPx * mDominateThresholdPx;
        mPhase = DragPhase.NONE;
    }

    // TileGroup.TileDragDelegate implementation.
    @Override
    public void onTileTouchDown(
            View view, MotionEvent event, TileGroup.TileDragHandlerDelegate dragHandlerDelegate) {
        assert event.getAction() == MotionEvent.ACTION_DOWN;
        if (!((TileView) view).isDraggable()) {
            return;
        }

        resetInternal(false);
        mPhase = DragPhase.PREPARE;
        mTileDragSession =
                new TileDragSession(
                        this, dragHandlerDelegate, (TileView) view, event.getX(), event.getY());

        mTimer.startTimer(
                START_DURATION_MS,
                () -> {
                    if (mTileDragSession == null) {
                        assert mPhase == DragPhase.NONE;
                        resetInternal(false);
                    } else {
                        cancelPendingChange();
                        mPhase = DragPhase.START;
                        // Needed to do this caller to consistently receive ACTION_MOVE.
                        mMvTilesLayout.requestDisallowInterceptTouchEvent(true);
                        mTileDragSession.start();
                    }
                });
    }

    @Override
    public void onSessionTileTouch(View view, MotionEvent event) {
        assert ((TileView) view).isDraggable() && mTileDragSession != null;

        if (event.getAction() == MotionEvent.ACTION_MOVE) {
            if (mPhase == DragPhase.START) {
                float dragDisplacementSquared =
                        mTileDragSession.getDragDisplacementSquared(event.getX(), event.getY());
                if (dragDisplacementSquared >= mDominateThresholdPxSquared) {
                    mTileDragSession.getTileDragHandlerDelegate().onDragDominate();
                    mPhase = DragPhase.DOMINATE;
                } else {
                    mTileDragSession.updateFromView(event.getX());
                }
            }
            if (mPhase == DragPhase.DOMINATE) {
                mTileDragSession.updateFromView(event.getX());
                mTileDragSession.updateToIndexAndAnimate();
            }

        } else if (event.getAction() == MotionEvent.ACTION_UP) {
            resetInternal(mPhase == DragPhase.DOMINATE);
            mPhase = DragPhase.NONE;

        } else if (event.getAction() == MotionEvent.ACTION_CANCEL) {
            resetInternal(false);
            mPhase = DragPhase.NONE;
        }
    }

    @Override
    public boolean hasSession() {
        return mPhase != DragPhase.NONE;
    }

    @Override
    public void reset() {
        resetInternal(false);
        mPhase = DragPhase.NONE;
    }

    // TileDragSession.Delegate implementation.
    @Override
    public float getTileWidthPx() {
        return mTileWidthPx;
    }

    @Override
    public List<TileView> getDraggableTileViews() {
        List<TileView> draggableTileViews = new ArrayList<TileView>();
        int tileCount = mMvTilesLayout.getTileCount();
        for (int i = 0; i < tileCount; ++i) {
            TileView tileView = mMvTilesLayout.getTileAt(i);
            if (tileView.isDraggable()) {
                draggableTileViews.add(tileView);
            }
        }
        return draggableTileViews;
    }

    @Override
    public SiteSuggestion getTileViewData(TileView view) {
        return mMvTilesLayout.getTileViewData(view);
    }

    private void cancelPendingChange() {
        if (mPendingChangeCanceller != null) {
            mPendingChangeCanceller.run();
            mPendingChangeCanceller = null;
        }
    }

    private void resetInternal(boolean accept) {
        mMvTilesLayout.requestDisallowInterceptTouchEvent(false);
        cancelPendingChange();
        mTimer.cancelTimer();
        if (mTileDragSession != null) {
            mPendingChangeCanceller = mTileDragSession.finish(accept);
            mTileDragSession = null;
        }
    }
}
