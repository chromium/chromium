// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.content.Context;
import android.util.AttributeSet;
import android.util.DisplayMetrics;

import androidx.annotation.VisibleForTesting;
import androidx.coordinatorlayout.widget.CoordinatorLayout;

import org.chromium.base.MathUtils;
import org.chromium.base.SysUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.tab_management.TabHoverCardView;
import org.chromium.ui.base.LocalizationUtils;

/** Strip-specific implementation of {@link TabHoverCardView}. */
@NullMarked
public class StripTabHoverCardView extends TabHoverCardView {

    public StripTabHoverCardView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Show the strip tab hover card.
     *
     * @param hoveredTab The {@link Tab} instance of the hovered tab.
     * @param isSelectedTab Whether the hovered tab is selected, {@code true} if the hovered tab is
     *     also the selected tab, {@code false} otherwise.
     * @param tabX To compute hover card positioning.
     * @param tabWidth To compute hover card positioning.
     * @param height The height of the tab strip stack.
     * @param topPadding The top padding applied to the tab strip, in dp.
     */
    public void show(
            @Nullable Tab hoveredTab,
            boolean isSelectedTab,
            float tabX,
            float tabWidth,
            float height,
            float topPadding) {
        if (hoveredTab == null) return;

        float[] position = getHoverCardPosition(isSelectedTab, tabX, tabWidth, height, topPadding);
        super.show(hoveredTab, position[0], position[1]);
    }

    /**
     * Get the x and y coordinates of the position of the hover card, in px.
     *
     * @param isSelectedTab Whether the tab is the selected tab, {@code true} if the hovered tab is
     *     also the selected tab, {@code false} otherwise.
     * @param tabX The tab x-position to compute hover card positioning.
     * @param tabWidth The tab width to compute hover card positioning.
     * @param height The height of the strip stack, to determine the y position of the card.
     * @param topPadding The top padding applied to the tab strip, in dp.
     * @return A float array specifying the x (array[0]) and y (array[1]) coordinates of the
     *     position of the hover card, in px.
     */
    @VisibleForTesting
    float[] getHoverCardPosition(
            boolean isSelectedTab, float tabX, float tabWidth, float height, float topPadding) {
        // 1. Determine the window width.
        DisplayMetrics displayMetrics = getContext().getResources().getDisplayMetrics();
        float displayDensity = displayMetrics.density;
        float windowWidthPx = displayMetrics.widthPixels;
        float windowWidthDp = windowWidthPx / displayDensity;

        // 2. Determine the hover card width, making adjustments relative to the window width if
        // applicable.
        float hoverCardWidthPx =
                getContext().getResources().getDimension(R.dimen.tab_hover_card_width);

        // Hover card width should be a maximum of 90% of the window width.
        hoverCardWidthPx = Math.min(hoverCardWidthPx, HOVER_CARD_MAX_WIDTH_PERCENT * windowWidthPx);
        float hoverCardWidthDp = hoverCardWidthPx / displayDensity;
        // Update the card LayoutParams if an adjustment on the current width is required.
        var layoutParams = getLayoutParams();
        if (hoverCardWidthPx != layoutParams.width) {
            setLayoutParams(
                    new CoordinatorLayout.LayoutParams(
                            Math.round(hoverCardWidthPx), layoutParams.height));
        }

        // 3. Determine the horizontal position of the hover card.
        float hoverCardXDp =
                LocalizationUtils.isLayoutRtl() ? (tabX - (hoverCardWidthDp - tabWidth)) : tabX;
        // Adjust the inactive folio tab hover card to align with the tab container
        // edge.
        if (!isSelectedTab) {
            hoverCardXDp +=
                    MathUtils.flipSignIf(
                            getContext()
                                            .getResources()
                                            .getDimension(R.dimen.inactive_tab_hover_card_x_offset)
                                    / displayDensity,
                            LocalizationUtils.isLayoutRtl());
        }

        // On a low-end device adjust the card to account for the shadow length of the background
        // drawable.
        if (SysUtils.isLowEndDevice()) {
            hoverCardXDp -=
                    getContext().getResources().getDimension(R.dimen.tab_hover_card_elevation)
                            / displayDensity;
        }

        float windowHorizontalMarginDp =
                getContext()
                                .getResources()
                                .getDimension(R.dimen.tab_hover_card_window_horizontal_margin)
                        / displayDensity;
        // Align the hover card at a minimum horizontal margin of 8dp from the window left edge.
        if (hoverCardXDp < windowHorizontalMarginDp) {
            hoverCardXDp = windowHorizontalMarginDp;
        }
        // Align the hover card at a minimum horizontal margin of 8dp from the window right edge.
        if (hoverCardXDp + hoverCardWidthDp > windowWidthDp - windowHorizontalMarginDp) {
            hoverCardXDp = windowWidthDp - hoverCardWidthDp - windowHorizontalMarginDp;
        }

        // 4. Determine the vertical position of the hover card.
        float hoverCardYDp = height + topPadding;

        // On a low-end device adjust the card to account for the shadow length of the background
        // drawable.
        if (SysUtils.isLowEndDevice()) {
            hoverCardYDp -=
                    getContext().getResources().getDimension(R.dimen.tab_hover_card_elevation)
                            / displayDensity;
        }

        return new float[] {hoverCardXDp * displayDensity, hoverCardYDp * displayDensity};
    }
}
