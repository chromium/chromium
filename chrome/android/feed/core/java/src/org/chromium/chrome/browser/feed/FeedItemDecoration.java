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

    private final FeedSurfaceCoordinator mCoordinator;
    private final Drawable mTopRoundedBackground;
    private final Drawable mBottomRoundedBackground;
    private final Drawable mBottomLeftRoundedBackground;
    private final Drawable mBottomRightRoundedBackground;
    private final Drawable mNotRoundedBackground;
    private final int mGutterPadding;
    private final int mAdditionalBottomCardPadding;

    public FeedItemDecoration(
            Context context,
            FeedSurfaceCoordinator coordinator,
            DrawableProvider drawableProvider,
            int gutterPadding) {
        mCoordinator = coordinator;

        mTopRoundedBackground =
                drawableProvider.getDrawable(R.drawable.home_surface_ui_background_top_rounded);
        mNotRoundedBackground =
                drawableProvider.getDrawable(R.drawable.home_surface_ui_background_not_rounded);
        mBottomRoundedBackground =
                drawableProvider.getDrawable(R.drawable.home_surface_ui_background_bottom_rounded);
        if (mCoordinator.useStaggeredLayout()) {
            mBottomLeftRoundedBackground =
                    drawableProvider.getDrawable(
                            R.drawable.home_surface_ui_background_bottomleft_rounded);
            mBottomRightRoundedBackground =
                    drawableProvider.getDrawable(
                            R.drawable.home_surface_ui_background_bottomright_rounded);
        } else {
            mBottomLeftRoundedBackground = null;
            mBottomRightRoundedBackground = null;
        }
        mGutterPadding = gutterPadding;
        mAdditionalBottomCardPadding =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.feed_containment_bottom_card_padding);
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

            // The last card comes with the divider which may overlap the bottom edge of
            // the feed containment. To work around this, we add an additional bottom padding to
            // the card.
            if (isLastViewInFeedContainment(position)) {
                bounds.bottom += mAdditionalBottomCardPadding;
            }

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
        boolean reachFullSpanBottomViewInFeedContainment = false;

        // Scan the child views until finding the one that is laid in multi-column. `i` is the index
        // that points to such view after the for loop.
        int indexOfFirstViewInMultiColumn = 0;
        for (;
                indexOfFirstViewInMultiColumn < parent.getChildCount();
                ++indexOfFirstViewInMultiColumn) {
            View child = parent.getChildAt(indexOfFirstViewInMultiColumn);
            int position = parent.getChildAdapterPosition(child);

            // Skip the non-feed elements.
            if (!belongsToFeedContainment(position)) {
                continue;
            }

            // Skip the views which take the full span, like section header and sign-in promo.
            int columnIndex = getColumnIndex(child);
            if (columnIndex != -1) {
                break;
            }
        }
        // Continue checking the child views that are laid in multi-column in order to find the
        // bottom position of each column.
        for (int i = indexOfFirstViewInMultiColumn; i < parent.getChildCount(); ++i) {
            View child = parent.getChildAt(i);
            int position = parent.getChildAdapterPosition(child);
            int columnIndex = getColumnIndex(child);

            // There may be full-span view, like loading more indicator, beyond the card views
            // laid in multi-column. If so, don't need to go further.
            if (columnIndex == -1) {
                reachFullSpanBottomViewInFeedContainment = true;
                break;
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

            // Don't need to go further if reaching the last view in feed containment.
            if (isLastViewInFeedContainment(position)) {
                reachLastViewInFeedContainment = true;
                break;
            }
        }
        int minBottom = Math.min(leftColumnBottom, rightColumnBottom);
        int maxBottom = Math.max(leftColumnBottom, rightColumnBottom);
        // We may just show the feed in single column even staggered layout is used, like the
        // following feed. When this occurs, minBottom is 0.
        boolean multiColumn = minBottom > 0;

        // Now update the bounds and draw the background.
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
            int columnIndex = getColumnIndex(child);
            if (multiColumn) {
                if (columnIndex != -1) {
                    if (columnIndex == 0) {
                        // For the card in the left column, include the gutter space.
                        bounds.right += 2 * mGutterPadding;
                    }

                    // For the bottom card in the shorter column, expand it to match the bottom card
                    // in the other column.
                    if ((reachLastViewInFeedContainment || reachFullSpanBottomViewInFeedContainment)
                            && minBottom != maxBottom
                            && bounds.bottom == minBottom) {
                        bounds.bottom += maxBottom - minBottom;
                    }

                    // The last card comes with the divider which may overlap the bottom edge of
                    // the feed containment if the last card is in the longer column. To work around
                    // this, we add an additional bottom padding to both bottom cards.
                    if (reachLastViewInFeedContainment && bounds.bottom == maxBottom) {
                        bounds.bottom += mAdditionalBottomCardPadding;
                    }
                }
            }

            // Add an additional bottom padding for the last card that takes the full span.
            if ((columnIndex == -1 || !multiColumn) && isLastViewInFeedContainment(position)) {
                bounds.bottom += mAdditionalBottomCardPadding;
            }

            // Draw the background for the extended bounds.
            Drawable background;
            if (multiColumn
                    && reachLastViewInFeedContainment
                    && bounds.bottom == maxBottom + mAdditionalBottomCardPadding) {
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

    // Returns the column index of the view in the staggered layout. Returns -1 if the view
    // takes the full span.
    private int getColumnIndex(View view) {
        return mCoordinator.getHybridListRenderer().getListLayoutHelper().getColumnIndex(view);
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
        } else if (isLastViewInFeedContainment(position)) {
            return mBottomRoundedBackground;
        } else {
            return mNotRoundedBackground;
        }
    }

    int getAdditionalBottomCardPaddingForTesting() {
        return mAdditionalBottomCardPadding;
    }
}
