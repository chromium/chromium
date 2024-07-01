// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import org.chromium.base.MathUtils;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.ui.base.LocalizationUtils;

/**
 * An interface that defines how to stack views and how they should look visually. This lets certain
 * components customize how the {@link StripLayoutHelper} functions and how other {@link Layout}s
 * visually order tabs.
 */
public abstract class StripStacker {
    /**
     * This gives the implementing class a chance to determine how the tabs should be ordered
     * visually. The positioning logic is the same regardless, this just has to do with visual
     * stacking.
     *
     * @param selectedIndex The selected index of the tabs.
     * @param indexOrderedTabs A list of tabs ordered by index.
     * @param outVisualOrderedTabs The new list of tabs, ordered from back (low z-index) to front
     *     (high z-index) visually.
     */
    public void createVisualOrdering(
            int selectedIndex,
            StripLayoutTab[] indexOrderedTabs,
            StripLayoutTab[] outVisualOrderedTabs) {
        // TODO(crbug.com/40268645): Stacking order can be ignored for TSR.
        assert indexOrderedTabs.length == outVisualOrderedTabs.length;

        selectedIndex = MathUtils.clamp(selectedIndex, 0, indexOrderedTabs.length);

        int outIndex = 0;
        for (int i = 0; i < selectedIndex; i++) {
            outVisualOrderedTabs[outIndex++] = indexOrderedTabs[i];
        }

        for (int i = indexOrderedTabs.length - 1; i >= selectedIndex; --i) {
            outVisualOrderedTabs[outIndex++] = indexOrderedTabs[i];
        }
    }

    /**
     * Computes and sets the draw X, draw Y, visibility and content offset for each view.
     *
     * @param indexOrderedViews A list of tabs ordered by index.
     * @param tabClosing Whether a tab is being closed.
     * @param groupTitleSlidingAnimRunning Whether a group title is sliding for reorder.
     * @param cachedTabWidth Whether The ideal tab width.
     */
    public abstract void setViewOffsets(
            StripLayoutView[] indexOrderedViews,
            boolean tabClosing,
            boolean groupTitleSlidingAnimRunning,
            float cachedTabWidth);

    /**
     * Computes the X offset for the new tab button.
     *
     * @param indexOrderedTabs A list of tabs ordered by index.
     * @param tabOverlapWidth The amount tabs overlap.
     * @param stripLeftMargin The left margin of the tab strip.
     * @param stripRightMargin The right margin of the tab strip.
     * @param stripWidth The width of the tab strip.
     * @param buttonWidth The width of the new tab button.
     * @return The x offset for the new tab button.
     */
    public float computeNewTabButtonOffset(
            StripLayoutTab[] indexOrderedTabs,
            float tabOverlapWidth,
            float stripLeftMargin,
            float stripRightMargin,
            float stripWidth,
            float buttonWidth) {
        return LocalizationUtils.isLayoutRtl()
                ? computeNewTabButtonOffsetRtl(
                        indexOrderedTabs,
                        stripLeftMargin,
                        stripRightMargin,
                        stripWidth,
                        buttonWidth)
                : computeNewTabButtonOffsetLtr(
                        indexOrderedTabs,
                        tabOverlapWidth,
                        stripLeftMargin,
                        stripRightMargin,
                        stripWidth);
    }

    private float computeNewTabButtonOffsetLtr(
            StripLayoutTab[] indexOrderedTabs,
            float tabOverlapWidth,
            float stripLeftMargin,
            float stripRightMargin,
            float stripWidth) {
        float rightEdge = stripLeftMargin;
        for (StripLayoutTab tab : indexOrderedTabs) {
            if (tab.isDying() || tab.isDraggedOffStrip()) continue;
            float layoutWidth = (tab.getWidth() - tabOverlapWidth) * tab.getWidthWeight();
            rightEdge = Math.max(tab.getDrawX() + layoutWidth, rightEdge);
        }

        // The draw X position for the new tab button is the rightEdge of the tab strip.
        return Math.min(rightEdge + tabOverlapWidth, stripWidth - stripRightMargin);
    }

    private float computeNewTabButtonOffsetRtl(
            StripLayoutTab[] indexOrderedTabs,
            float stripLeftMargin,
            float stripRightMargin,
            float stripWidth,
            float newTabButtonWidth) {
        float leftEdge = stripWidth - stripRightMargin;
        for (StripLayoutTab tab : indexOrderedTabs) {
            if (tab.isDying() || tab.isDraggedOffStrip()) continue;
            leftEdge = Math.min(tab.getDrawX(), leftEdge);
        }

        // The draw X position for the new tab button is the left edge of the tab strip minus
        // the new tab button width.
        return Math.max(leftEdge, stripLeftMargin) - newTabButtonWidth;
    }

    /**
     * Performs an occlusion pass, setting the visibility on tabs. This is relegated to this
     * interface because the implementing class knows the proper visual order to optimize this pass.
     *
     * @param indexOrderedViews A list of views ordered by index.
     * @param xOffset The xOffset for the start of the strip.
     * @param visibleWidth The width of the visible space on the tab strip.
     */
    public abstract void performOcclusionPass(
            StripLayoutView[] indexOrderedViews, float xOffset, float visibleWidth);
}
