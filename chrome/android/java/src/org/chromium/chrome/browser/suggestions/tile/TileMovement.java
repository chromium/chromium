// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import android.view.ViewPropertyAnimator;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.widget.tile.TileView;

import java.util.ArrayList;
import java.util.List;

/**
 * Manages transient movement and animation of a row of {@link TileView} instances that are being
 * reordered under drag-and-drop UI. When tiles move, only their (visual) locations are affected,
 * and not their order within the parent container. Once tile movement has been reject or accepted,
 * all positions are restored; a refresh is needed to actually move tiles.
 */
@NullMarked
public class TileMovement {

    private final List<TileView> mTileViews;
    private final List<Float> mOriginalX;
    private final List<@Nullable ViewPropertyAnimator> mAnimators;
    private boolean mIsActive;
    private int mAnimateBarrier;

    /**
     * @param tileViews A non-empty row of tiles to manage. The X-coordinates are assumed to ascend
     *     (LTR) or descend (RTL).
     */
    TileMovement(List<TileView> tileViews) {
        assert !tileViews.isEmpty();
        mTileViews = tileViews;
        mOriginalX = new ArrayList<>();
        mAnimators = new ArrayList<@Nullable ViewPropertyAnimator>();
        for (TileView tileView : mTileViews) {
            float x = tileView.getX();
            mOriginalX.add(x);
            mAnimators.add(null);
        }
        mIsActive = true;
    }

    /**
     * @return The {@link TileView} at {@param index}.
     */
    public TileView getTileViewAt(int index) {
        return mTileViews.get(index);
    }

    /**
     * @return Index of {@param view} existing in {@link #mTileViews}.
     */
    public int getIndexOfView(TileView view) {
        int index = mTileViews.indexOf(view);
        assert index >= 0;
        return index;
    }

    /**
     * @return Index of the {@link #mTileViews} element whose X-coordinate is nearest to {@param x},
     *     preferring lower index on a tie.
     */
    public int getIndexOfViewNearestTo(float x) {
        int n = mTileViews.size();
        int bestIndex = 0;
        float bestDist = Math.abs(mOriginalX.get(0) - x);
        for (int i = 1; i < n; i++) {
            float dist = Math.abs(mOriginalX.get(i) - x);
            if (bestDist > dist) {
                bestDist = dist;
                bestIndex = i;
            }
        }
        return bestIndex;
    }

    /**
     * @return The minimum X-coordinate among managed tiles.
     */
    public float getXLo() {
        return Math.min(mOriginalX.get(0), mOriginalX.get(mOriginalX.size() - 1));
    }

    /**
     * @return The maximum X-coordinate among managed tiles.
     */
    public float getXHi() {
        return Math.max(mOriginalX.get(0), mOriginalX.get(mOriginalX.size() - 1));
    }

    /**
     * Changes the (visual) location of the tile at {@param index} to the original location of tile
     * at {@param newIndex}. Once move completes (possibly after animation, as determined by {@param
     * isAnimated}), runs {@param onEnd} if non-null. Replaces the previous tile animation, and
     * prevents the previously specified {@param onEnd} from running.
     */
    public void moveTile(int index, int newIndex, boolean isAnimated, @Nullable Runnable onEnd) {
        TileView tileView = mTileViews.get(index);
        float x = mOriginalX.get(newIndex);
        ViewPropertyAnimator oldAnimator = mAnimators.get(index);
        if (oldAnimator != null) {
            oldAnimator.cancel();
            mAnimators.set(index, null);
        }
        if (isAnimated) {
            ViewPropertyAnimator animator = tileView.animate();
            mAnimators.set(index, animator);
            if (onEnd != null) {
                animator.withEndAction(onEnd);
            }
            animator.x(x).start();
        } else {
            tileView.setX(x);
            if (onEnd != null) {
                onEnd.run();
            }
        }
    }

    /**
     * Given that tile at {@param fromIndex} is moved to {@param toIndex}, shifts the latter and all
     * tiles in between towards {@param fromIndex}, and resets all other tiles, with animation that
     * overrides existing animations.
     */
    public void shiftBackgroundTile(int fromIndex, int toIndex) {
        int n = mTileViews.size();
        // Shift affected tiles if {@link #toIndex} < {@link #fromIndex}.
        for (int i = toIndex; i < fromIndex; i++) {
            moveTile(i, i + 1, /* isAnimated= */ true, /* onEnd= */ null);
        }
        // Shift affected tiles if {@link #toIndex} > {@link #fromIndex}.
        for (int i = toIndex; i > fromIndex; i--) {
            moveTile(i, i - 1, /* isAnimated= */ true, /* onEnd= */ null);
        }
        // Reset tiles that are unaffected / no longer affected.
        for (int i = Math.min(fromIndex, toIndex) - 1; i >= 0; i--) {
            moveTile(i, i, /* isAnimated= */ true, /* onEnd= */ null);
        }
        for (int i = Math.max(fromIndex, toIndex) + 1; i < n; i++) {
            moveTile(i, i, /* isAnimated= */ true, /* onEnd= */ null);
        }
    }

    /**
     * Accepts tile movement from {@param fromIndex} to {@param toIndex}, starting with animation
     * for aesthetics. The animation can still be cancelled by calling cancel(). When animation
     * finishes, runs {@param onAccept}.
     */
    public void animatedAccept(int fromIndex, int toIndex, Runnable onAccept) {
        // Animate the "from" tile moving to "to" tile, which can be cancelled via cancelIfActive().
        moveTile(
                fromIndex,
                toIndex,
                /* isAnimated= */ true,
                () -> {
                    // Animation completes: Reset all tile positions, and run {@param onAccept}.
                    // Note that reset is needed even on accept, since {@link TileVIew} changes are
                    // "for show", and the actual movement is done on reload, which in turn can
                    // reuse TileView instances (so it's important to clean up).
                    // Next, would restore cause "glitch", i.e., tiles temporarily jump back, before
                    // being re-rendered into the desired state? We assume this won't happen
                    // (perhaps since @{param onAccept} is eager), or that any effect is negligible
                    // and not worth fixing for now.
                    cancelIfActive();
                    onAccept.run();
                });
    }

    /**
     * Reject tile movement, so move tiles back to original locations, with animation. This can
     * hastened by calling {@link cancelIfActive}.
     */
    public void animatedReject() {
        // While animation is running, {@link cancelIfActive()} can be called to skip to end. Once
        // animation finishes then we deactivate it by clearing {@link #mIsActive}.
        mAnimateBarrier = mTileViews.size();
        Runnable onEnd =
                () -> {
                    if (--mAnimateBarrier == 0) {
                        mIsActive = false;
                    }
                };
        for (int i = 0; i < mTileViews.size(); i++) {
            moveTile(i, i, true, onEnd);
        }
    }

    /** Stops all pending animations and restores original tile locations without animation. */
    public void cancelIfActive() {
        if (mIsActive) {
            resetTiles();
            mIsActive = false;
        }
    }

    /** Restores original tile locations, without animation. */
    private void resetTiles() {
        for (int i = 0; i < mTileViews.size(); i++) {
            ViewPropertyAnimator oldAnimator = mAnimators.get(i);
            if (oldAnimator != null) {
                oldAnimator.cancel();
                mAnimators.set(i, null);
            }
            mTileViews.get(i).setX(mOriginalX.get(i));
        }
    }
}
