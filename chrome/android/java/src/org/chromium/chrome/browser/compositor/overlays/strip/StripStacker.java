// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.TAB_OVERLAP_WIDTH_DP;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.MathUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton;
import org.chromium.ui.base.LocalizationUtils;

/**
 * An interface that defines how to stack views and how they should look visually. This lets certain
 * components customize how the {@link StripLayoutHelper} functions and how other {@link Layout}s
 * visually order tabs.
 */
@NullMarked
public abstract class StripStacker {
    // On tablets (48dp button touch target), we can't fully shift the NTB left without affecting
    // touch target, so we apply a small actual offset(4dp) and also rely on a visual shift(6dp) in
    // the CC layer instead. On desktop (32dp touch target), we have more room to apply a real
    // offset(10dp) directly. No more visual offset needed for desktop.
    private static final float NEW_TAB_BUTTON_X_OFFSET_TOWARDS_TABS =
            StripLayoutUtils.shouldApplyMoreDensity() ? 10.f : 4.f;

    /**
     * Sets the draw properties for the new tab button.
     *
     * @param newTabButton The {@link StripLayoutView} for the new tab button.
     * @param indexOrderedTabs A list of tabs ordered by index.
     * @param stripLeftMargin The left margin of the tab strip.
     * @param stripRightMargin The right margin of the tab strip.
     * @param stripWidth The width of the tab strip.
     * @param buttonWidth The width of the new tab button.
     * @param tabStripFull {@code True} if the views take up the entire width of the strip.
     */
    public void pushDrawPropertiesToButtons(
            CompositorButton newTabButton,
            StripLayoutTab[] indexOrderedTabs,
            float stripLeftMargin,
            float stripRightMargin,
            float stripWidth,
            float buttonWidth,
            boolean tabStripFull) {
        // 1. The NTB is faded out upon entering reorder mode and hidden when the model is empty.
        boolean isEmpty = indexOrderedTabs.length == 0;
        newTabButton.setVisible(!isEmpty);
        if (isEmpty) return;

        // 2. Get idealX.
        // Note: This method anchors the NTB to either a static position at the end of the strip OR
        // right next to the final tab in the strip. This only WAI if the final view in the strip is
        // guaranteed to be a tab. If this changes (e.g. we allow empty tab groups), then this will
        // need to be updated.
        float idealX =
                computeNewTabButtonIdealX(
                        indexOrderedTabs,
                        TAB_OVERLAP_WIDTH_DP,
                        stripLeftMargin,
                        stripRightMargin,
                        stripWidth,
                        buttonWidth,
                        tabStripFull);

        // 3. Position the new tab button.
        newTabButton.setDrawX(idealX + newTabButton.getOffsetX());
    }

    /** See {@link #pushDrawPropertiesToButtons}. */
    @VisibleForTesting
    float computeNewTabButtonIdealX(
            StripLayoutTab[] indexOrderedTabs,
            float tabOverlapWidth,
            float stripLeftMargin,
            float stripRightMargin,
            float stripWidth,
            float buttonWidth,
            boolean tabStripFull) {
        // TODO(crbug.com/376525967): Pull overlap width from utils constant instead of passing in.
        float idealX =
                LocalizationUtils.isLayoutRtl()
                        ? computeNewTabButtonIdealXRtl(
                                indexOrderedTabs,
                                stripLeftMargin,
                                stripRightMargin,
                                stripWidth,
                                buttonWidth)
                        : computeNewTabButtonIdealXLtr(
                                indexOrderedTabs,
                                tabOverlapWidth,
                                stripLeftMargin,
                                stripRightMargin,
                                stripWidth);
        return adjustNewTabButtonIdealXIfFull(idealX, tabStripFull);
    }

    private float computeNewTabButtonIdealXLtr(
            StripLayoutTab[] indexOrderedTabs,
            float tabOverlapWidth,
            float stripLeftMargin,
            float stripRightMargin,
            float stripWidth) {
        float rightEdge = stripLeftMargin;
        for (StripLayoutTab tab : indexOrderedTabs) {
            if (StripLayoutUtils.skipTabEdgePositionCalculation(tab)) continue;
            float layoutWidth = (tab.getWidth() - tabOverlapWidth) * tab.getWidthWeight();
            rightEdge = Math.max(tab.getDrawX() + layoutWidth, rightEdge);
        }

        // The draw X position for the new tab button is the rightEdge of the tab strip.
        return Math.min(rightEdge + tabOverlapWidth, stripWidth - stripRightMargin);
    }

    private float computeNewTabButtonIdealXRtl(
            StripLayoutTab[] indexOrderedTabs,
            float stripLeftMargin,
            float stripRightMargin,
            float stripWidth,
            float newTabButtonWidth) {
        float leftEdge = stripWidth - stripRightMargin;
        for (StripLayoutTab tab : indexOrderedTabs) {
            if (StripLayoutUtils.skipTabEdgePositionCalculation(tab)) continue;
            leftEdge = Math.min(tab.getDrawX(), leftEdge);
        }

        // The draw X position for the new tab button is the left edge of the tab strip minus
        // the new tab button width.
        return Math.max(leftEdge, stripLeftMargin) - newTabButtonWidth;
    }

    private float adjustNewTabButtonIdealXIfFull(float idealX, boolean tabStripFull) {
        if (tabStripFull) return idealX;
        // Move NTB close to tabs by 4 dp when tab strip is not full.
        boolean isLtr = !LocalizationUtils.isLayoutRtl();
        return idealX + MathUtils.flipSignIf(NEW_TAB_BUTTON_X_OFFSET_TOWARDS_TABS, isLtr);
    }

    /**
     * Performs an occlusion pass, setting the visibility on tabs. This is relegated to this
     * interface because the implementing class knows the proper visual order to optimize this pass.
     *
     * @param indexOrderedViews A list of views ordered by index.
     * @param xOffset The xOffset for the start of the strip.
     * @param visibleWidth The width of the visible space on the tab strip.
     */
    public abstract void pushDrawPropertiesToViews(
            StripLayoutView[] indexOrderedViews, float xOffset, float visibleWidth);
}
