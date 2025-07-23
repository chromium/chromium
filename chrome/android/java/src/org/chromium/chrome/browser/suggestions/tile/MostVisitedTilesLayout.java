// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.view.View;
import android.widget.HorizontalScrollView;

import androidx.annotation.Px;

import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.user_education.IphCommand;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.widget.tile.TileView;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.ui.base.DeviceFormFactor;

/** The most visited tiles layout. */
@NullMarked
public class MostVisitedTilesLayout extends TilesLinearLayout {

    private final boolean mIsTablet;
    private final @Px int mTileViewWidthPx;
    private final @Px int mIntervalPaddingsTablet;
    private final @Px int mEdgePaddingsTablet;
    private final @Px int mTileViewDividerWidth;
    private Integer mInitialTileCount;
    private Integer mInitialChildCount;
    private @Nullable Integer mTileToMoveInViewIdx;
    private @Nullable Runnable mTriggerIphTask;
    private @Nullable SuggestionsTileVerticalDivider mDivider;
    private @Nullable Integer mDividerIndex;

    /** Constructor for inflating from XML. */
    public MostVisitedTilesLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        mIsTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(context);

        Resources resources = getResources();
        mTileViewWidthPx = resources.getDimensionPixelOffset(R.dimen.tile_view_width);
        mIntervalPaddingsTablet =
                resources.getDimensionPixelSize(R.dimen.tile_view_padding_interval_tablet);
        mEdgePaddingsTablet =
                resources.getDimensionPixelSize(R.dimen.tile_view_padding_edge_tablet);
        mTileViewDividerWidth = resources.getDimensionPixelSize(R.dimen.tile_view_divider_width);
    }

    @Override
    public void removeAllViews() {
        super.removeAllViews();
        mDivider = null;
        mDividerIndex = null;
    }

    @Override
    void setIntervalMargins(@Px int margin) {
        super.setIntervalMargins(margin);

        if (mDivider != null) {
            assert mDividerIndex != null;
            // Let M = margin, W = divider width, A = adjusted margin. Desired apparent margin that
            // pretends divider absence is M = A + W + A, so A = (M - W) / 2.
            @Px int adjustedMargin = (margin - mTileViewDividerWidth) / 2;
            // Update the start margins of the divider and its next sibling.
            updateViewStartMargin(mDivider, adjustedMargin);
            View childAfterDivider = getChildAt(mDividerIndex + 1);
            if (childAfterDivider != null) {
                updateViewStartMargin(childAfterDivider, adjustedMargin);
            }
        }
    }

    @Override
    public void addNonTileViewWithWidth(View view, float widthDp) {
        super.addNonTileViewWithWidth(view, widthDp);
        if (mDivider != null) return;

        // Take the first divider found and assume it's the only one.
        if (view instanceof SuggestionsTileVerticalDivider divider) {
            mDivider = divider;
            mDividerIndex = getChildCount() - 1;
            mDivider.hide(/* isAnimated= */ false);
        }
    }

    void destroy() {
        for (int i = 0; i < getTileCount(); i++) {
            TileView tileView = getTileAt(i);
            tileView.setOnClickListener(null);
            tileView.setOnCreateContextMenuListener(null);
        }
        removeAllViews();
    }

    public SiteSuggestion getTileViewData(TileView tileView) {
        return ((SuggestionsTileView) tileView).getData();
    }

    /**
     * Adjusts the edge margin of the tile elements when they are displayed in the center of the NTP
     * on the tablet.
     *
     * @param totalWidth The width of the mv tiles container.
     */
    void updateEdgeMarginTablet(int totalWidth) {
        boolean isFullFilled =
                totalWidth
                                - mTileViewWidthPx * mInitialTileCount
                                - getNonTileViewsTotalWidthPx()
                                - mIntervalPaddingsTablet * (mInitialChildCount - 1)
                                - 2 * mEdgePaddingsTablet
                        >= 0;
        if (!isFullFilled) {
            // When splitting the window, this function is invoked with a different totalWidth value
            // during the process. Therefore, we must update the edge padding with the appropriate
            // value once the correct totalWidth is provided at the end of the split.
            setEdgeMargins(mEdgePaddingsTablet);
            return;
        }

        int tileCount = getTileCount();
        int childCount = getChildCount();
        int edgeMargin =
                (totalWidth
                                - mTileViewWidthPx * tileCount
                                - getNonTileViewsTotalWidthPx()
                                - mIntervalPaddingsTablet * (childCount - 1))
                        / 2;
        setEdgeMargins(edgeMargin);
    }

    /**
     * Tags the {@link TileView} at {@param tileIdx} so that on next Layout, minimal scroll is
     * performed to ensure it's in-view.
     */
    void ensureTileIsInViewOnNextLayout(int tileIdx) {
        // If a value exists, simply overwrite it since the old value is likely new irrelevant.
        mTileToMoveInViewIdx = tileIdx;
    }

    /**
     * Attempts to show the in-product help for MVT Customization "Pin this shortcut" feature. At
     * least one tile must exist, since help is anchored on the first. Must be called before layout
     * takes place.
     */
    public void triggerCustomizationIph(UserEducationHelper userEducationHelper) {
        if (getTileCount() == 0) return;

        // Defer until layout, so that the first TileView can be used as the the anchor.
        mTriggerIphTask =
                () -> {
                    TileView firstTileView = getTileAt(0);
                    IphCommand command =
                            new IphCommandBuilder(
                                            getResources(),
                                            FeatureConstants.MOST_VISITED_TILES_CUSTOMIZATION_PIN,
                                            R.string.ntp_custom_links_help_pin,
                                            R.string.ntp_custom_links_help_pin)
                                    .setAnchorView(firstTileView)
                                    .setInsetRect(new Rect(0, 0, 0, 0))
                                    .build();
                    userEducationHelper.requestShowIph(command);
                };
    }

    @Override
    @Initializer
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        if (mInitialTileCount == null) {
            mInitialTileCount = getTileCount();
        }
        if (mInitialChildCount == null) {
            mInitialChildCount = getChildCount();
        }
        if (mIsTablet) {
            updateEdgeMarginTablet(widthMeasureSpec);
        }
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    @Override
    public void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);
        if (mTileToMoveInViewIdx != null) {
            Integer scrollXPx = getScrollXToMakeTileVisible(mTileToMoveInViewIdx);
            if (scrollXPx != null) {
                HorizontalScrollView parent = (HorizontalScrollView) getParent();
                parent.smoothScrollTo(scrollXPx.intValue(), 0);
            }
            mTileToMoveInViewIdx = null;
        }

        if (mTriggerIphTask != null) {
            mTriggerIphTask.run();
            mTriggerIphTask = null;
        }
    }

    public HorizontalScrollView getScrollView() {
        return (HorizontalScrollView) getParent();
    }

    public @Nullable SuggestionsTileVerticalDivider getDividerMaybeNull() {
        return mDivider;
    }

    private @Nullable Integer getScrollXToMakeTileVisible(int tileIdx) {
        if (tileIdx >= getTileCount()) {
            return null;
        }
        HorizontalScrollView scrollView = getScrollView();
        @Px float tileXPx = getTileAt(tileIdx).getX();
        @Px int scrollXPx = scrollView.getScrollX();
        // If scroll position is too high so that the tile is out-of-view / truncated, scroll left
        // so that the tile appears on the left edge (RTL doesn't matter).
        @Px int scrollXHiPx = (int) tileXPx;
        if (scrollXPx > scrollXHiPx) {
            return scrollXHiPx;
        }
        // If scroll position is too low so that the tile is out-of-view / truncated, scroll right
        // so that the tile appears on the right edge (RTL doesn't matter).
        @Px int scrollXLoPx = (int) (tileXPx + mTileViewWidthPx - scrollView.getWidth());
        if (scrollXPx < scrollXLoPx) {
            return scrollXLoPx;
        }
        // Entire tile is in-view; no scroll is needed.
        return null;
    }
}
