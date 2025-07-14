// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import android.os.Handler;

import androidx.annotation.Px;

import org.chromium.base.MathUtils;
import org.chromium.build.annotations.NullMarked;

/**
 * Manages (horizontal) scroll dynamics of a Tile undergoing drag-and-drop. Referring to
 * TileDragSession comments, this involves nested Views Outer &lt;- Inner &lt;- Tile.
 *
 * <p>Outer's left and right ends contain "sense zones". When Tile is being dragged, its position is
 * passed to this class. Dragging to a sense zone triggers auto-scroll. This in turn causes Inner to
 * shift relative to Outer (handled by this class). Moreover, per drag-and-drop UI expectation, Tile
 * should appear stationary. Therefore Tile should also counter-shift relative to Inner (handled by
 * the caller).
 *
 * <p>The above movement does not trigger UI events. For auto-scroll to continue, a timer is used to
 * iteratively trigger movement.
 */
@NullMarked
class TileDragAutoScroll {

    /** Delegate to retrieve get scroll-related data and set scroll. */
    interface Delegate {
        /** Returns scroll X, i.e., Outer X relative to Inner, in PX. */
        @Px
        int getScrollInnerX();

        /** Returns active (being dragged) Tile X relative to Inner, in PX. */
        @Px
        float getActiveTileX();

        /** Changes scroll X by {@param dx}. */
        void scrollInnerXBy(@Px int dx);

        /** Callback to inform on each attempt to update scroll (by {@param dx}). */
        void onAutoScroll(@Px int dx);
    }

    // Sense zone start: The distance (as multiple of Tile width) that the dragged Tile needs to
    // extend beyond Outer's left / right boundaries to trigger scroll.
    private static final float SCROLL_SENSE_X_START_RATIO = 0.1f;

    // Sense zone width: The additional incursion (as multiple of Tile width) into the sense zone
    // before saturating at max scroll speed.
    private static final float SCROLL_SENSE_WIDTH_RATIO = 0.2f;

    // Time delay between successive auto-scroll frames, in ms. 33 ms results in 30 Hz, but this is
    // okay since smoothScrollBy() can be used to make high-end devices scroll smoothly.
    private static final int SCROLL_DELAY_MS = 33;

    // Relative auto-scroll speed, in tiles per ms.
    private static final float SCROLL_SPEED_RATIO = 5f / 1000f; // (tiles / s) / (ms / s).

    private final Delegate mDelegate;
    private final Handler mHandler;
    private final @Px float mSenseWidth;

    // Relative to Outer.
    private final @Px float mSenseXLo;
    private final @Px float mSenseXHi;

    // Relative to Inner.
    private final @Px float mScrollXLo;
    private final @Px float mScrollXHi;

    private final @Px float mMaxScrollDx;

    /**
     * @param autoScrollDelegate Delegate to retrieve additional info.
     * @param handler Handler for delayed run() loop.
     * @param outerWidth Width of Outer, to define sense zones.
     * @param tileWidth Width of Tile, to define bounds and to act as scaling factor.
     * @param innerXLo Minimum allowed Tile X relative to Inner.
     * @param innerXHi Maximum allowed Tile X relative to Inner.
     */
    public TileDragAutoScroll(
            Delegate autoScrollDelegate,
            Handler handler,
            @Px float outerWidth,
            @Px float tileWidth,
            @Px float tileXLo,
            @Px float tileXHi) {
        mDelegate = autoScrollDelegate;
        mHandler = handler;
        mSenseWidth = SCROLL_SENSE_WIDTH_RATIO * tileWidth;
        float scrollSenseDx = SCROLL_SENSE_X_START_RATIO * tileWidth;
        mSenseXLo = -scrollSenseDx;
        mSenseXHi = outerWidth - tileWidth + scrollSenseDx;
        mScrollXLo = tileXLo;
        mScrollXHi = tileXHi - outerWidth + tileWidth;
        mMaxScrollDx = Math.max(1f, tileWidth * SCROLL_SPEED_RATIO * SCROLL_DELAY_MS);
    }

    /** Easing function from displacement ratio in the sense zone to speed ratio. */
    private static float easing(float ratio) {
        return ratio * ratio;
    }

    /**
     * Computes a [-1, 1] capped dead-zone function that encodes auto-scroll activation (true iff
     * non-0), direction (sign), and speed fraction (magnitude).
     */
    private float computeScrollFactor(float tileOuterX) {
        //  tileOuterX      clampedTileX       deadZoneTileX         (returned)
        //        /                                      /
        //       /                 /---                 /                    /---
        //      /       -         /        =       /---/       ->       /---/
        //     /              ---/                /                 ---/
        //    /                                  /
        float clampedTileX = MathUtils.clamp(tileOuterX, mSenseXLo, mSenseXHi);
        float deadZoneTileX = tileOuterX - clampedTileX;
        if (deadZoneTileX == 0f) return 0f; // Not in sense zones.

        float ratio = Math.min(1f, Math.abs(deadZoneTileX / mSenseWidth)); // In [0, 1].
        return Math.signum(deadZoneTileX) * easing(ratio);
    }

    /**
     * Checks the conditions for auto-scroll to run. If satisfied, stats or continues an auto-scroll
     * frame, and queues a self-call after {@link SCROLL_DELAY_MS} delay.
     */
    public void run() {
        // Determine whether tile is at sense zone, stop if false.
        int scrollX = mDelegate.getScrollInnerX();
        float tileOuterX = mDelegate.getActiveTileX() - scrollX;
        float scrollFactor = computeScrollFactor(tileOuterX);
        if (scrollFactor == 0f) {
            return; // Implicit stop().
        }

        // Detect whether scroll boundary is hit, stop if so.
        float idealScrollDx = scrollFactor * mMaxScrollDx;
        float newScrollX = MathUtils.clamp(scrollX + idealScrollDx, mScrollXLo, mScrollXHi);
        int scrollDx = Math.round(newScrollX - scrollX);
        if (scrollDx == 0) {
            return; // Implicit stop().
        }

        mDelegate.scrollInnerXBy(scrollDx);
        // Let caller handle Tile counter-shift and other complex behavior.
        mDelegate.onAutoScroll(scrollDx);

        mHandler.postDelayed(this::run, SCROLL_DELAY_MS);
    }

    /** Stops pending auto-scroll frame, or no-op if non-existent. */
    public void stop() {
        mHandler.removeCallbacksAndMessages(null);
    }
}
