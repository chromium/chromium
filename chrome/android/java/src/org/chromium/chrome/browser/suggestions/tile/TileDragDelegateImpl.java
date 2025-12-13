// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import android.content.res.Resources;
import android.view.MotionEvent;
import android.view.View;
import android.widget.HorizontalScrollView;

import androidx.annotation.IntDef;
import androidx.annotation.Px;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.tile.TileDragDelegate.ReorderFlow;
import org.chromium.components.browser_ui.widget.tile.TileView;
import org.chromium.ui.util.RunnableTimer;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/**
 * TileDragDelegate implementation. The two main flows are handled by the following:
 *
 * <ol>
 *   <li>Drag Flow: Uses {@link TileDragSession} to wrap a {@link TileMovement}, and manages
 *       per-session states with state machine defined by {@link DragPhase}.
 *   <li>Swap Flow: Uses {@link TileMovement} directly.
 * </ol>
 *
 * <p>The instance is reusable across multiple sessions for a {@link MostVisitedTilesLayout}
 * instance. UI race condition (i.e., user moves too fast and performs action while the previous
 * action is undergoing animation) is handled by having a new session cancel the previous one.
 */
@NullMarked
class TileDragDelegateImpl implements TileDragDelegate, TileDragSession.Delegate {

    // Drag Flow tile drag session dynamics uses a state machine. Here are its states.
    @IntDef({
        DragPhase.NONE,
        DragPhase.PREPARE,
        DragPhase.START,
        DragPhase.DOMINATE,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface DragPhase {
        // NONE: No drag. ACTION_DOWN => :=PREPARE (i.e., enter the PREPARE phase).
        int NONE = 0;

        // PREPARE: No drag. Default touch handling triggers ACTION_UP => tile click; ACTION_MOVE
        // (after small drag) => scroll. These default interactions trigger ACTION_CANCEL => :=NONE.
        // If PREPARE persists longer than "start duration" then :=START. TileDragSession is
        // instantiated here.
        int PREPARE = 1;

        // START: Tile drag is live: ACTION_MOVE => move "from" tile; ACTION_UP => cancel drag
        // :=NONE. If drag displacement exceeds "dominate threshold" then :=DOMINATE.
        int START = 2;

        // DOMINATE: Tile drag is live: ACTION_MOVE => move "from" and background tiles; ACTION_UP
        // => finalize (potentially triggering operation and refresh), and then :=NONE.
        int DOMINATE = 3;

        int NUM_ENTRIES = 4;
    }

    // "Start duration": Delay (in ms) for triggering PREPARE -> START change.
    private static final long START_DURATION_MS = 300;

    // Relative "dominate threshold": Multiplied by tile width to get "dominate threshold".
    private static final float DOMINATE_TRESHOLD_RATIO = 0.4f;

    // Parent container for dragged tiles.
    private final MostVisitedTilesLayout mMvTilesLayout;

    private final @Px float mTileWidthPx;

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

    // Main state for Swap Flow.
    private @Nullable TileMovement mTileMovementForSwap;

    // Runnable to finalize the action of the previous flow. If called before the final animation
    // completes, then the call may cancel (e.g., for Drag Flow, to reduce confusion) or complete
    // (e.g., for Swap Flow, to allow fast repeated keyboard-triggered swaps) the action.
    private @Nullable Runnable mPendingChangeFinalizer;

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
            View view, MotionEvent event, TileDragSession.EventListener eventListener) {
        assert event.getAction() == MotionEvent.ACTION_DOWN;
        if (!((TileView) view).isDraggable()) {
            return;
        }

        reset();
        mPhase = DragPhase.PREPARE;
        mTileDragSession =
                new TileDragSession(
                        this, eventListener, (TileView) view, event.getX(), event.getY());

        mTimer.startTimer(
                START_DURATION_MS,
                () -> {
                    if (mTileDragSession == null) {
                        assert mPhase == DragPhase.NONE;
                        reset();
                    } else {
                        finalizePendingChange();
                        mPhase = DragPhase.START;
                        // Needed to do this caller to consistently receive ACTION_MOVE.
                        mMvTilesLayout.requestDisallowInterceptTouchEvent(true);
                        mTileDragSession.start(new TileMovement(getDraggableTileViews()));
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
                    mPhase = DragPhase.DOMINATE;
                    mTileDragSession.dominate();
                } else {
                    mTileDragSession.updateFromView(event.getX());
                }
            }
            if (mPhase == DragPhase.DOMINATE) {
                mTileDragSession.updateFromView(event.getX());
            }

        } else if (event.getAction() == MotionEvent.ACTION_UP) {
            // {@link reset()} consumes {@link #mPendingChangeFinalizer} and clears
            // {@link #mTileDragSession}! Therefore we saving {@link #mTileDragSession} beforehand,
            // and use it to assign {@link #mPendingChangeFinalizer} afterwards.
            boolean accept = (mPhase == DragPhase.DOMINATE);
            TileDragSession savedTileDragSession = mTileDragSession;
            reset();
            mPendingChangeFinalizer = savedTileDragSession.finish(accept);

        } else if (event.getAction() == MotionEvent.ACTION_CANCEL) {
            reset();
        }
    }

    @Override
    public void swapTiles(
            View fromView, int toDeltaIndex, TileDragSession.EventListener eventListener) {
        List<TileView> tileViews = getDraggableTileViews();
        int fromIndex = tileViews.indexOf(fromView);
        if (fromIndex < 0) {
            return;
        }
        int toIndex = fromIndex + toDeltaIndex;
        if (toIndex < 0 || toIndex >= tileViews.size()) {
            return;
        }

        reset();
        assert mTileMovementForSwap == null && mPendingChangeFinalizer == null;

        // Temporarily increment Z so the "from" tile is drawn on top of other tiles.
        float savedFromZ = fromView.getZ();
        fromView.setZ(savedFromZ + 1.0f);

        mTileMovementForSwap = new TileMovement(tileViews);
        View toView = tileViews.get(toIndex);

        // Save {@link #mTileMovementForSwap} since it maybe cleared on the next reset(). Having it
        // bound in a local variable
        TileMovement savedTileMovementForSwap = mTileMovementForSwap;
        mPendingChangeFinalizer =
                () -> {
                    // Not really cancelling; just reset visuals.
                    savedTileMovementForSwap.cancelIfActive();
                    // Always apply change, (and unlike Drag Flow) even if interrupted during
                    // animation. This is because keyboard repeat can occur faster than animation.
                    eventListener.onReorderAccept(
                            ReorderFlow.SWAP_FLOW,
                            getTileViewData((TileView) fromView),
                            getTileViewData((TileView) toView));
                    fromView.setZ(savedFromZ);
                };

        mTileMovementForSwap.moveTile(
                toIndex, fromIndex, /* isAnimated= */ true, /* onEnd= */ null);
        mTileMovementForSwap.animatedAccept(
                fromIndex, toIndex, /* onAccept= */ this::finalizePendingChange);
    }

    @Override
    public boolean hasTileDragSession() {
        return mPhase != DragPhase.NONE;
    }

    @Override
    public void showDivider(boolean isAnimated) {
        SuggestionsTileVerticalDivider divider = mMvTilesLayout.getDividerMaybeNull();
        if (divider != null) {
            divider.show(isAnimated);
        }
    }

    @Override
    public void hideDivider(boolean isAnimated) {
        SuggestionsTileVerticalDivider divider = mMvTilesLayout.getDividerMaybeNull();
        if (divider != null) {
            divider.hide(isAnimated);
        }
    }

    @Override
    public void reset() {
        // Clean up in-flight states.
        if (mTileDragSession != null) {
            mMvTilesLayout.requestDisallowInterceptTouchEvent(false);
        }
        finalizePendingChange();
        mTimer.cancelTimer();
        mPhase = DragPhase.NONE;

        // Clear main flow variables.
        mTileDragSession = null;
        mTileMovementForSwap = null;
    }

    @Override
    public boolean isFirstDraggableTile(View tileView) {
        List<TileView> draggableTiles = getDraggableTileViews();
        return !draggableTiles.isEmpty() && draggableTiles.get(0) == tileView;
    }

    @Override
    public boolean isLastDraggableTile(View tileView) {
        List<TileView> draggableTiles = getDraggableTileViews();
        return !draggableTiles.isEmpty()
                && draggableTiles.get(draggableTiles.size() - 1) == tileView;
    }

    // TileDragSession.Delegate implementation.
    @Override
    public boolean isAutoScrollEnabled() {
        // TODO(b/431765443): Return !DeviceInfo.isDesktop() if auto-scroll should be disabled on
        // desktop Android browsers.
        return true;
    }

    @Override
    public @Px float getTileWidth() {
        return mTileWidthPx;
    }

    @Override
    public SiteSuggestion getTileViewData(TileView view) {
        return mMvTilesLayout.getTileViewData(view);
    }

    @Override
    public HorizontalScrollView getOuterView() {
        return mMvTilesLayout.getScrollView();
    }

    List<TileView> getDraggableTileViews() {
        List<TileView> draggableTileViews = new ArrayList<>();
        int tileCount = mMvTilesLayout.getTileCount();
        for (int i = 0; i < tileCount; ++i) {
            TileView tileView = mMvTilesLayout.getTileAt(i);
            if (tileView.isDraggable()) {
                draggableTileViews.add(tileView);
            }
        }
        return draggableTileViews;
    }

    private void finalizePendingChange() {
        if (mPendingChangeFinalizer != null) {
            mPendingChangeFinalizer.run();
            mPendingChangeFinalizer = null;
        }
    }
}
