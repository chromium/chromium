// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.components.browser_ui.widget.tile.TileView;

import java.util.List;

/**
 * Helper for {@link TileDragDelegateImpl} to manage session states and interface with {@link
 * TileMovement}. W.r.t. {@link TileDragDelegateImpl.DragPhase}, the class is used in {PREPARE,
 * START, DOMINATE}.
 */
@NullMarked
class TileDragSession {

    // Delegate to retrieve data from {@link TileDragDelegateImpl}.
    interface Delegate {
        /**
         * @return Width of a Most Visit Tile, in PX.
         */
        float getTileWidthPx();

        /**
         * @return The current list of {@link TileView} instances in the MVT container.
         */
        List<TileView> getDraggableTileViews();

        /**
         * @return The {@link SiteSuggestion} corresponding to a {@link TileView}.
         */
        SiteSuggestion getTileViewData(TileView view);
    }

    // Scaling factor to shrink the "from" tile in {START, DOMINATE}.
    private static final float DRAG_ACTIVE_SCALE = 0.8f;

    // Relative X-margin: Multiplied by tile width to get the X-margin.
    private static final float DRAG_X_MARGIN_RATIO = 0.2f;

    private final Delegate mDelegate;
    private final TileGroup.TileDragHandlerDelegate mDragResultDelegate;
    private final TileView mFromView;
    private final float mStartX;
    private final float mStartY;

    // Variables unneeded during PREPARE are initialized in start().
    private @Nullable TileMovement mTileMovement;
    private float mSavedSrcX;
    private float mSavedSrcZ;
    private int mFromIndex;
    private int mToIndex;
    private float mDxLo;
    private float mDxHi;

    /**
     * @param delegate Data provider.
     * @param dragHandlerDelegate Delegate to respond to events and results.
     * @param fromView The view of the "from" tile that's being dragged.
     * @param eventX The X coordinate of the initial ACTION_DOWN event on the "from" tile.
     * @param eventY The Y coordinate of the same.
     */
    public TileDragSession(
            Delegate delegate,
            TileGroup.TileDragHandlerDelegate dragHandlerDelegate,
            TileView fromView,
            float eventX,
            float eventY) {
        mDelegate = delegate;
        mDragResultDelegate = dragHandlerDelegate;
        mFromView = fromView;
        mStartX = fixEventX(eventX);
        mStartY = eventY;
    }

    @Initializer
    public void start() {
        mTileMovement = new TileMovement(mDelegate.getDraggableTileViews());
        mSavedSrcX = mFromView.getX();
        mSavedSrcZ = mFromView.getZ();
        mFromIndex = mTileMovement.getIndexOfView(mFromView);
        mToIndex = mFromIndex;

        // X-margin: A dragged tile is constrained to stay in the box containing all draggable
        // tiles. To soften the constraint, the X-margin specifies extra room for the dragged tile
        // to travel horizontally beyond the box.
        float dragXMarginPx = DRAG_X_MARGIN_RATIO * mDelegate.getTileWidthPx();
        mDxLo = mTileMovement.getXLo() - mSavedSrcX - dragXMarginPx;
        mDxHi = mTileMovement.getXHi() - mSavedSrcX + dragXMarginPx;

        mFromView.animate().scaleX(DRAG_ACTIVE_SCALE).scaleY(DRAG_ACTIVE_SCALE).start();
        // Temporarily increment Z so the "from" tile is drawn on top of other tiles.
        mFromView.setZ(mSavedSrcZ + 1.0f);
    }

    public TileGroup.TileDragHandlerDelegate getTileDragHandlerDelegate() {
        return mDragResultDelegate;
    }

    /**
     * Updates {@link mFromView} movement on ACTION_MOVE.
     *
     * @param eventX The X coordinate of the The ACTION_MOVE event on the "from" tile.
     */
    public void updateFromView(float eventX) {
        // {@param eventX} is relative to translation X, so we need to add it back to compensate.
        float rawDx = fixEventX(eventX) - mStartX;
        mFromView.setTranslationX(Math.max(mDxLo, Math.min(rawDx, mDxHi)));
    }

    /**
     * @param eventX The X coordinate of the The ACTION_MOVE event on the "from" tile.
     * @param eventY The Y coordinate of same.
     * @return The Euclidean distance squared from ({@link mStartX}, {@link mStartY}) to ({@param
     *     eventX}, {@param eventY}).
     */
    public float getDragDisplacementSquared(float eventX, float eventY) {
        float rawDx = fixEventX(eventX) - mStartX;
        float rawDy = eventY - mStartY;
        // Not using Math.hypot() since it may be slow.
        return rawDx * rawDx + rawDy * rawDy;
    }

    /** Updates {@link mToIndex} and possibly shift background tiles on drag. */
    public void updateToIndexAndAnimate() {
        assert mTileMovement != null;

        // Find the draggable tile that's closest to the "from" tile's current location.
        // This is robust for LTR and RTL. Use linear search, which is fast enough.
        int newToIndex = mTileMovement.getIndexOfViewNearestTo(mFromView.getX());

        // If {@link #mToIndex} changes: Shift non-"from" tiles towards the "from" tile.
        if (mToIndex != newToIndex) {
            mToIndex = newToIndex;
            mTileMovement.shiftBackgroundTile(mFromIndex, mToIndex);
        }
    }

    /**
     * Finishes the drag-and-drop session. If animation is in flight, also returns the {@link
     * mTileMovement} instance so animation can be cancelled; otherwise returns null.
     *
     * @param accept Whether to accept the drag-and-drop if the "from" tile is dragged to a
     *     different "to" tile.
     * @return A {@link Runnable} that can be called to cancel the accept / reject animation and
     *     action while it's in -flight. Once complete then the calling would have no effect.
     */
    public @Nullable Runnable finish(boolean accept) {
        mFromView.setZ(mSavedSrcZ);
        mFromView.animate().scaleX(1.0f).scaleY(1.0f).start();

        // Handle the case where function is called before start() is called.
        if (mTileMovement == null) {
            return null;
        }

        if (accept && mFromIndex != mToIndex) {
            mTileMovement.animatedAccept(
                    mFromIndex,
                    mToIndex,
                    /* onAccept= */ () -> {
                        if (mTileMovement != null) {
                            TileView toView = mTileMovement.getTileViewAt(mToIndex);
                            mDragResultDelegate.onDragAccept(
                                    mDelegate.getTileViewData(mFromView),
                                    mDelegate.getTileViewData(toView));
                        }
                    });

        } else {
            mTileMovement.animatedReject();
        }

        return () -> {
            if (mTileMovement != null) {
                mTileMovement.cancelIfActive();
            }
            mFromView.setScaleX(1.0f);
            mFromView.setScaleY(1.0f);
        };
    }

    /** Helper to instantiate {@link TileMovement}, extract to method to allow testing override. */
    protected TileMovement createTileMovement(List<TileView> tileViews) {
        return new TileMovement(tileViews);
    }

    /**
     * Performs correction to {@param eventX} from a fresh {@plink MotionEvent} for {@link
     * #mFromView}. This is needed because the X value read is relative to
     * `mFromView.getTranslationX()`, but we'd like the X relative to the container.
     */
    private float fixEventX(float eventX) {
        return eventX + mFromView.getTranslationX();
    }
}
