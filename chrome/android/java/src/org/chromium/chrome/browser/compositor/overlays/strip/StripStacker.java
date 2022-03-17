// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import org.chromium.base.MathUtils;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.ui.base.LocalizationUtils;

/**
 * An interface that defines how to stack tabs and how they should look visually.  This lets
 * certain components customize how the {@link StripLayoutHelper} functions and how other
 * {@link Layout}s visually order tabs.
 */
public abstract class StripStacker {
    /**
     * @return Whether or not the close button can be shown.  Note that even if it can be shown,
     *         it might not be due to how much of the tab is actually visible to preserve proper hit
     *         target sizes.
     */
    public boolean canShowCloseButton() {
        return true;
    }

    /**
     * This gives the implementing class a chance to determine how the tabs should be ordered
     * visually. The positioning logic is the same regardless, this just has to do with visual
     * stacking.
     *
     * @param selectedIndex The selected index of the tabs.
     * @param indexOrderedTabs A list of tabs ordered by index.
     * @param outVisualOrderedTabs The new list of tabs, ordered from back (low z-index) to front
     *                             (high z-index) visually.
     */
    public void createVisualOrdering(int selectedIndex, StripLayoutTab[] indexOrderedTabs,
            StripLayoutTab[] outVisualOrderedTabs) {
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
     * Computes and sets the draw X, draw Y, visibility and content offset for each tab.
     *
     * @param selectedIndex The selected index of the tabs.
     * @param indexOrderedTabs A list of tabs ordered by index.
     * @param tabStackWidth The width of a tab when it's stacked behind another tab.
     * @param maxTabsToStack The maximum number of tabs to stack.
     * @param tabOverlapWidth The amount tabs overlap.
     * @param stripLeftMargin The left margin of the tab strip.
     * @param stripRightMargin The right margin of the tab strip.
     * @param stripWidth The width of the tab strip.
     * @param inReorderMode Whether the strip is in reorder mode.
     */
    public abstract void setTabOffsets(int selectedIndex, StripLayoutTab[] indexOrderedTabs,
            float tabStackWidth, int maxTabsToStack, float tabOverlapWidth, float stripLeftMargin,
            float stripRightMargin, float stripWidth, boolean inReorderMode);

    /**
     * Computes the X offset for the new tab button.
     *
     * @param indexOrderedTabs A list of tabs ordered by index.
     * @param tabOverlapWidth The amount tabs overlap.
     * @param stripLeftMargin The left margin of the tab strip.
     * @param stripRightMargin The right margin of the tab strip.
     * @param stripWidth The width of the tab strip.
     * @param mNewTabButtonWidth The width of the new tab button.
     * @return The x offset for the new tab button.
     */
    public float computeNewTabButtonOffset(StripLayoutTab[] indexOrderedTabs, float tabOverlapWidth,
            float stripLeftMargin, float stripRightMargin, float stripWidth,
            float mNewTabButtonWidth) {
        return LocalizationUtils.isLayoutRtl()
                ? computeNewTabButtonOffsetRtl(indexOrderedTabs, tabOverlapWidth, stripLeftMargin,
                        stripRightMargin, stripWidth, mNewTabButtonWidth)
                : computeNewTabButtonOffsetLtr(indexOrderedTabs, tabOverlapWidth, stripLeftMargin,
                        stripRightMargin, stripWidth);
    }

    private float computeNewTabButtonOffsetLtr(StripLayoutTab[] indexOrderedTabs,
            float tabOverlapWidth, float stripLeftMargin, float stripRightMargin,
            float stripWidth) {
        float rightEdge = stripLeftMargin;

        for (StripLayoutTab tab : indexOrderedTabs) {
            float layoutWidth = (tab.getWidth() - tabOverlapWidth) * tab.getWidthWeight();
            rightEdge = Math.max(tab.getDrawX() + layoutWidth, rightEdge);
        }

        rightEdge = Math.min(rightEdge + tabOverlapWidth, stripWidth - stripRightMargin);

        // Adjust the right edge by the tab overlap width so that the new tab button is nestled
        // closer to the tab.
        rightEdge -= tabOverlapWidth / 2;

        // The draw X position for the new tab button is the rightEdge of the tab strip.
        return rightEdge;
    }

    private float computeNewTabButtonOffsetRtl(StripLayoutTab[] indexOrderedTabs,
            float tabOverlapWidth, float stripLeftMargin, float stripRightMargin, float stripWidth,
            float newTabButtonWidth) {
        float leftEdge = stripWidth - stripRightMargin;

        for (StripLayoutTab tab : indexOrderedTabs) {
            leftEdge = Math.min(tab.getDrawX(), leftEdge);
        }

        leftEdge = Math.max(leftEdge, stripLeftMargin);

        // Adjust the left edge by the tab overlap width so that the new tab button is nestled
        // closer to the tab.
        leftEdge += tabOverlapWidth / 2;

        // The draw X position for the new tab button is the left edge of the tab strip minus
        // the new tab button width.
        return leftEdge - newTabButtonWidth;
    }

    /**
     * Performs an occlusion pass, setting the visibility on tabs. This is relegated to this
     * interface because the implementing class knows the proper visual order to optimize this pass.
     * @param selectedIndex The selected index of the tabs.
     * @param indexOrderedTabs A list of tabs ordered by index.
     * @param stripWidth The width of the tab strip.
     */
    public abstract void performOcclusionPass(int selectedIndex, StripLayoutTab[] indexOrderedTabs,
            float stripWidth);
}