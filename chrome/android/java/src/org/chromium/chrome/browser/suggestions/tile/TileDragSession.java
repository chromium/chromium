// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import android.os.Handler;
import android.widget.HorizontalScrollView;

import androidx.annotation.Px;

import org.chromium.base.MathUtils;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.tile.TileDragDelegate.ReorderFlow;
import org.chromium.components.browser_ui.widget.tile.TileView;

import java.util.List;

/**
 * Helper for {@link TileDragDelegateImpl} to manage session states and interface with {@link
 * TileMovement}. W.r.t. {@link TileDragDelegateImpl.DragPhase}, the class is used in {PREPARE,
 * START, DOMINATE}. During a drag session, 3 key (nested) Views are:
 *
 * <ul>
 *   <li>Outer: Immobile container where everything happens, can get and set scroll value.
 *   <li>Inner: Large content that's partially visible, and needs scrolling to be viewed.
 *   <li>Tile: Fixed-width View being repositioned via active drag UI.
 * </ul>
 */
@NullMarked
class TileDragSession implements TileDragAutoScroll.Delegate {

    /** Delegate to retrieve data from {@link TileDragDelegateImpl}. */
    interface Delegate {
        /** Returns whether scroll should take place when a tile is dragged to edge of Outer. */
        boolean isAutoScrollEnabled();

        /** Returns the width of a Most Visit Tile, in PX. */
        @Px
        float getTileWidth();

        /** Returns the {@link SiteSuggestion} corresponding to a {@link TileView}. */
        SiteSuggestion getTileViewData(TileView view);

        /** Returns the Outer View. */
        HorizontalScrollView getOuterView();
    }

    /** Listener for drag-and-drop UI stages, and to receive the final result. */
    interface EventListener {
        /** Called when the tile drag session starts. */
        void onDragStart();

        /**
         * Called when the tile drag session becomes the dominant UI mode. The implementation should
         * suppress competing UI, e.g., context menu.
         */
        void onDragDominate();

        /**
         * Called when tile drag UI successfully produces result. The implementation should execute
         * the operation, then refresh UI if successful.
         *
         * @param reorderFlow Enum to identify the flow used to perform reordering.
         * @param fromSuggestion Data to identify the tile being dragged.
         * @param toSuggestion Data to identify the tile being dropped on.
         * @return Whether the operation successfully ran.
         */
        boolean onReorderAccept(
                @ReorderFlow int reorderFlow,
                SiteSuggestion fromSuggestion,
                SiteSuggestion toSuggestion);

        /** Called when the drag UI is cancelled (and no UI refresh takes place). */
        void onReorderCancel();
    }

    // Scaling factor to shrink the "from" tile in {START, DOMINATE}.
    private static final float DRAG_ACTIVE_SCALE = 0.8f;

    // Relative X-margin: Multiply by tile width to get the X-margin.
    private static final float DRAG_X_MARGIN_RATIO = 0.2f;

    private final Delegate mDelegate;
    private final EventListener mEventListener;
    private final TileView mFromView;
    private final float mSavedFromZ;
    private final @Px float mTileWidth;
    private final @Px float mStartX;
    private final @Px float mStartY;
    private final @Px float mAnchorX;

    // Variables unneeded during PREPARE are initialized in start().
    private @Nullable TileMovement mTileMovement;
    private @Nullable TileDragAutoScroll mAutoScroll;
    private int mFromIndex;
    private int mToIndex;
    private float mXLo;
    private float mXHi;

    /**
     * @param delegate Data provider.
     * @param eventListener Listener for events and to pass the final result.
     * @param fromView The view of the "from" tile that's being dragged.
     * @param eventX The X coordinate of the initial ACTION_DOWN event on the "from" tile.
     * @param eventY The Y coordinate of the same.
     */
    public TileDragSession(
            Delegate delegate,
            EventListener eventListener,
            TileView fromView,
            float eventX,
            float eventY) {
        mDelegate = delegate;
        mEventListener = eventListener;
        mFromView = fromView;
        mSavedFromZ = mFromView.getZ();
        mTileWidth = mDelegate.getTileWidth();
        mStartX = fixEventX(eventX);
        mStartY = eventY;
        mAnchorX = mStartX - mFromView.getX();
    }

    @Initializer
    public void start(TileMovement tileMovement) {
        mTileMovement = tileMovement;
        mFromIndex = mTileMovement.getIndexOfView(mFromView);
        mToIndex = mFromIndex;

        // X-margin: A dragged tile is constrained to stay in the box containing all draggable
        // tiles. To soften the constraint, the X-margin specifies extra room for the dragged tile
        // to travel horizontally beyond the box.
        float dragXMarginPx = DRAG_X_MARGIN_RATIO * mTileWidth;
        mXLo = mTileMovement.getXLo() - dragXMarginPx;
        mXHi = mTileMovement.getXHi() + dragXMarginPx;

        if (mDelegate.isAutoScrollEnabled()) {
            mAutoScroll =
                    new TileDragAutoScroll(
                            this,
                            new Handler(),
                            mDelegate.getOuterView().getWidth(),
                            mTileWidth,
                            mTileMovement.getXLo(),
                            mTileMovement.getXHi());
        }

        mFromView.animate().scaleX(DRAG_ACTIVE_SCALE).scaleY(DRAG_ACTIVE_SCALE).start();
        // Temporarily increment Z so the "from" tile is drawn on top of other tiles.
        mFromView.setZ(mSavedFromZ + 1.0f);

        mEventListener.onDragStart();
    }

    // Notifies the class that tile drag session has become the dominant UI mode.
    public void dominate() {
        mEventListener.onDragDominate();
    }

    // TileDragAutoScroll.Delegate implementation.
    @Override
    public @Px int getScrollInnerX() {
        return mDelegate.getOuterView().getScrollX();
    }

    @Override
    public @Px float getActiveTileX() {
        return mFromView.getX();
    }

    @Override
    public void scrollInnerXBy(@Px int dx) {
        mDelegate.getOuterView().smoothScrollBy(dx, 0);
    }

    @Override
    public void onAutoScroll(@Px int dx) {
        // Counter-shift "from" tile against scrolling so it appears stationary.
        mFromView.setX(MathUtils.clamp(mFromView.getX() + dx, mXLo, mXHi));

        updateToIndexAndAnimate();
    }

    /**
     * Updates {@link mFromView} movement on ACTION_MOVE.
     *
     * @param eventX The X coordinate of the The ACTION_MOVE event on the "from" tile.
     */
    public void updateFromView(float eventX) {
        mFromView.setX(MathUtils.clamp(fixEventX(eventX) - mAnchorX, mXLo, mXHi));
        updateToIndexAndAnimate();

        if (mAutoScroll != null) {
            // Terminate any residual auto-scroll loop, then potentially start new one, so that
            // scrolling would in step with pointer action.
            mAutoScroll.stop();
            mAutoScroll.run();
        }
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
     *     action while it's in-flight. Once complete then the calling would have no effect.
     */
    public @Nullable Runnable finish(boolean accept) {
        mFromView.setZ(mSavedFromZ);
        mFromView.animate().scaleX(1.0f).scaleY(1.0f).start();
        if (mAutoScroll != null) {
            mAutoScroll.stop();
        }

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
                            mEventListener.onReorderAccept(
                                    ReorderFlow.DRAG_FLOW,
                                    mDelegate.getTileViewData(mFromView),
                                    mDelegate.getTileViewData(toView));
                        }
                    });

        } else {
            mTileMovement.animatedReject();
            mEventListener.onReorderCancel();
        }

        return () -> {
            // Reset visuals if animation is in-flight.
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
     * Converts {@param eventX} from a fresh {@plink MotionEvent} from being relative to {@link
     * #mFromView} to relative to Inner.
     */
    private float fixEventX(float eventX) {
        return eventX + mFromView.getX();
    }
}
