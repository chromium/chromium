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
    private final @Px int mTileViewWidth;
    private final @Px int mIntervalPaddingsTablet;
    private final @Px int mEdgePaddingsTablet;
    private final @Px int mTileViewDividerWidth;
    private @Nullable Integer mTileToMoveInViewIdx;
    private @Nullable Runnable mTriggerIphTask;
    private @Nullable SuggestionsTileVerticalDivider mDivider;
    private @Nullable Integer mDividerIndex;

    /** Constructor for inflating from XML. */
    public MostVisitedTilesLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        mIsTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(context);

        Resources resources = getResources();
        mTileViewWidth = resources.getDimensionPixelSize(R.dimen.tile_view_width);
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
    public void addDivider(SuggestionsTileVerticalDivider divider) {
        super.addDivider(divider);
        if (mDivider != null) return;

        // Take the first divider found and assume it's the only one.
        mDivider = divider;
        mDividerIndex = getChildCount() - 1;
        mDivider.hide(/* isAnimated= */ false);
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
     * Returns whether all the tiles and non-tiles, with a small margin would fit within a container
     * with the given {@param totalWidth} without the need to scroll. For tablets only.
     */
    public boolean contentFitsOnTablet(int totalWidth) {
        return totalWidth >= 2 * mEdgePaddingsTablet + getTabletContentWidth();
    }

    /** Returns the total width of tiles, UI Views, and interval paddings. */
    @Px
    int getTabletContentWidth() {
        return (int)
                (mTileViewWidth * getTileCount()
                        + mUiViewsTotalWidth
                        + mIntervalPaddingsTablet * (mTileAndUiViewCount - 1));
    }

    /**
     * Adjusts the edge margin of the tile elements when they are displayed in the center of the NTP
     * on the tablet.
     *
     * @param totalWidth The width of the mv tiles container.
     */
    void updateEdgeMarginTablet(int totalWidth) {
        // If content fits within `totalWidth`, then return the required margin to center it. Else
        // scrolling would be needed, so return a fixed margin for the scrolled content.
        int edgeMargin =
                contentFitsOnTablet(totalWidth)
                        ? (totalWidth - getTabletContentWidth()) / 2
                        : mEdgePaddingsTablet;
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
        if (mIsTablet) {
            updateEdgeMarginTablet(MeasureSpec.getSize(widthMeasureSpec));
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
        @Px int scrollXLoPx = (int) (tileXPx + mTileViewWidth - scrollView.getWidth());
        if (scrollXPx < scrollXLoPx) {
            return scrollXLoPx;
        }
        // Entire tile is in-view; no scroll is needed.
        return null;
    }
}
