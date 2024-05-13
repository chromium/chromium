// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.recyclerview.widget.RecyclerView;

// Used to draw the background for the feed containment.
public class FeedItemDecoration extends RecyclerView.ItemDecoration {
    /** Allows to mock for testing purpose. */
    public interface DrawableProvider {
        Drawable getDrawable(int resId);
    }

    // This should be consistent with the gutter value defined in http://shortn/_ZVrTS16q0c.
    private static final int STAGGERED_GUTTER_COLUMN_PADDING = 12;

    private final FeedSurfaceCoordinator mCoordinator;
    private final Drawable mTopRoundedBackground;
    private final Drawable mBottomRoundedBackground;
    private final Drawable mBottomLeftRoundedBackground;
    private final Drawable mBottomRightRoundedBackground;
    private final Drawable mNotRoundedBackground;
    private final int mExtraPadding;

    public FeedItemDecoration(
            Context context,
            FeedSurfaceCoordinator coordinator,
            DrawableProvider drawableProvider) {
        mCoordinator = coordinator;

        mTopRoundedBackground =
                drawableProvider.getDrawable(R.drawable.home_surface_ui_background_top_rounded);
        mNotRoundedBackground =
                drawableProvider.getDrawable(R.drawable.home_surface_ui_background_not_rounded);
        if (mCoordinator.useStaggeredLayout()) {
            mBottomLeftRoundedBackground =
                    drawableProvider.getDrawable(
                            R.drawable.home_surface_ui_background_bottomleft_rounded);
            mBottomRightRoundedBackground =
                    drawableProvider.getDrawable(
                            R.drawable.home_surface_ui_background_bottomright_rounded);
            mBottomRoundedBackground = null;
            mExtraPadding =
                    context.getResources()
                            .getDimensionPixelSize(R.dimen.feed_containment_horizontal_padding);

        } else {
            mBottomRoundedBackground =
                    drawableProvider.getDrawable(
                            R.drawable.home_surface_ui_background_bottom_rounded);
            mBottomLeftRoundedBackground = null;
            mBottomRightRoundedBackground = null;
            mExtraPadding = 0;
        }
    }

    @Override
    public void onDraw(Canvas canvas, RecyclerView parent, RecyclerView.State state) {
        super.onDraw(canvas, parent, state);

        if (mCoordinator.useStaggeredLayout()) {
            drawBackgroundForStaggeredLayout(canvas, parent);
        } else {
            drawBackgroundForStandardLayout(canvas, parent);
        }
    }

    private void drawBackgroundForStandardLayout(Canvas canvas, RecyclerView parent) {
        for (int i = 0; i < parent.getChildCount(); ++i) {
            View child = parent.getChildAt(i);
            int position = parent.getChildAdapterPosition(child);

            // Skip the non-feed elements.
            if (!belongsToFeedContainment(position)) {
                continue;
            }

            Rect bounds = new Rect();
            parent.getDecoratedBoundsWithMargins(child, bounds);

            // Draw the background for the view.
            Drawable background = getBackgroundDrawable(position);
            background.setBounds(bounds);
            background.draw(canvas);
        }
    }

    private void drawBackgroundForStaggeredLayout(Canvas canvas, RecyclerView parent) {
        // When the feed is rendered in 2-column staggered layout, the total height of each
        // column may not be equal due to the unfilled empty space at the end of one column.
        // We need to find the bottom position of last card in each column so that we can
        // "expand" the last card of the shorter column to be on par with the other column.
        int leftColumnBottom = 0;
        int rightColumnBottom = 0;
        boolean reachLastViewInFeedContainment = false;
        for (int i = 0; i < parent.getChildCount(); ++i) {
            View child = parent.getChildAt(i);
            int position = parent.getChildAdapterPosition(child);

            // Skip the non-feed elements.
            if (!belongsToFeedContainment(position)) {
                continue;
            }

            // Skip the views which take the full span, like section header and sign-in promo.
            int columnIndex =
                    mCoordinator
                            .getHybridListRenderer()
                            .getListLayoutHelper()
                            .getColumnIndex(child);
            if (columnIndex == -1) {
                continue;
            }

            if (isLastViewInFeedContainment(position)) {
                reachLastViewInFeedContainment = true;
            }

            Rect bounds = new Rect();
            parent.getDecoratedBoundsWithMargins(child, bounds);

            if (columnIndex == 0) {
                if (bounds.bottom > leftColumnBottom) {
                    leftColumnBottom = bounds.bottom;
                }
            } else {
                if (bounds.bottom > rightColumnBottom) {
                    rightColumnBottom = bounds.bottom;
                }
            }
        }
        int minBottom = Math.min(leftColumnBottom, rightColumnBottom);
        int maxBottom = Math.max(leftColumnBottom, rightColumnBottom);

        for (int i = 0; i < parent.getChildCount(); ++i) {
            View child = parent.getChildAt(i);
            int position = parent.getChildAdapterPosition(child);

            // Skip the non-feed elements.
            if (!belongsToFeedContainment(position)) {
                continue;
            }

            Rect bounds = new Rect();
            parent.getDecoratedBoundsWithMargins(child, bounds);

            // Extend the bounds to compensate the gutter between columns and the unfilled empty
            // space.
            int columnIndex =
                    mCoordinator
                            .getHybridListRenderer()
                            .getListLayoutHelper()
                            .getColumnIndex(child);
            if (columnIndex == -1) {
                // For the full-span view, like section header or sign-in promo, we only need to
                // add extra padding on the right since the original right padding is not
                // enough.
                bounds.right += mExtraPadding;
            } else {
                if (columnIndex == 0) {
                    // For the card in the left column, include the gutter space.
                    bounds.right += 2 * STAGGERED_GUTTER_COLUMN_PADDING;
                } else {
                    // For the card at the right column, include the extra padding on the right.
                    bounds.right += mExtraPadding;
                }

                // For the bottom card in the shorter column, expand it to match the bottom card
                // in the other column.
                if (reachLastViewInFeedContainment
                        && minBottom != maxBottom
                        && bounds.bottom == minBottom) {
                    bounds.bottom += maxBottom - minBottom;
                }
            }

            // Draw the background for the extended bounds.
            Drawable background;
            if (reachLastViewInFeedContainment && bounds.bottom == maxBottom) {
                background =
                        (columnIndex == 0)
                                ? mBottomLeftRoundedBackground
                                : mBottomRightRoundedBackground;
            } else {
                background = getBackgroundDrawable(position);
            }
            background.setBounds(bounds);
            background.draw(canvas);
        }
    }

    private boolean belongsToFeedContainment(int position) {
        // Exclude the NTP header views that appear above the feed header and the last view
        // which is used to provide the bottom margin for the feed containment.
        return position >= mCoordinator.getSectionHeaderPosition()
                && position < mCoordinator.getContentManager().getItemCount() - 1;
    }

    private boolean isLastViewInFeedContainment(int position) {
        return position == mCoordinator.getContentManager().getItemCount() - 2;
    }

    private Drawable getBackgroundDrawable(int position) {
        if (position == mCoordinator.getSectionHeaderPosition()) {
            return mTopRoundedBackground;
        } else if (!mCoordinator.useStaggeredLayout() && isLastViewInFeedContainment(position)) {
            return mBottomRoundedBackground;
        } else {
            return mNotRoundedBackground;
        }
    }

    int getGutterPaddingForTesting() {
        return STAGGERED_GUTTER_COLUMN_PADDING * 2;
    }

    int getExtraPaddingForTesting() {
        return mExtraPadding;
    }
}
