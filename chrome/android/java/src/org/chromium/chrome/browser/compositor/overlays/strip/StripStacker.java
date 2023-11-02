// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import org.chromium.base.MathUtils;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
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
     * @param tabClosing Whether a tab is being closed.
     */
    public abstract void setTabOffsets(int selectedIndex, StripLayoutTab[] indexOrderedTabs,
            float tabStackWidth, int maxTabsToStack, float tabOverlapWidth, float stripLeftMargin,
            float stripRightMargin, float stripWidth, boolean inReorderMode, boolean tabClosing,
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
     * @param touchTargetOffset Touch target offset applied to the button position.
     * @return The x offset for the new tab button.
     */
    public float computeNewTabButtonOffset(StripLayoutTab[] indexOrderedTabs, float tabOverlapWidth,
            float stripLeftMargin, float stripRightMargin, float stripWidth, float buttonWidth,
            float touchTargetOffset, float cachedTabWidth, boolean animate) {
        return LocalizationUtils.isLayoutRtl()
                ? computeNewTabButtonOffsetRtl(indexOrderedTabs, stripLeftMargin, stripRightMargin,
                        stripWidth, buttonWidth, touchTargetOffset, cachedTabWidth, animate)
                : computeNewTabButtonOffsetLtr(indexOrderedTabs, tabOverlapWidth, stripLeftMargin,
                        stripRightMargin, stripWidth, touchTargetOffset, cachedTabWidth, animate);
    }

    private float computeNewTabButtonOffsetLtr(StripLayoutTab[] indexOrderedTabs,
            float tabOverlapWidth, float stripLeftMargin, float stripRightMargin, float stripWidth,
            float touchTargetOffset, float cachedTabWidth, boolean animate) {
        float rightEdge = stripLeftMargin;
        boolean tabStripImpEnabled = ChromeFeatureList.sTabStripImprovements.isEnabled();

        for (StripLayoutTab tab : indexOrderedTabs) {
            float tabWidth;
            float tabDrawX;
            float tabWidthWeight;
            if (tabStripImpEnabled && animate) {
                // This value is set to 1.f to avoid the new tab button jitter for the improved tab
                // strip design. The tab.width and tab.drawX may not reflect the final values before
                // the tab closing animations are completed.
                tabWidthWeight = 1.f;
                tabWidth = cachedTabWidth;
                tabDrawX = tab.getIdealX();
            } else {
                tabWidthWeight = tab.getWidthWeight();
                tabWidth = tab.getWidth();
                tabDrawX = tab.getDrawX();
            }
            float layoutWidth = (tabWidth - tabOverlapWidth) * tabWidthWeight;
            rightEdge = Math.max(tabDrawX + layoutWidth, rightEdge); // use idealX here
        }

        rightEdge = Math.min(rightEdge + tabOverlapWidth, stripWidth - stripRightMargin);

        // Adjust the new tab button to be away from the tabs to account for the touch target skew.
        rightEdge += touchTargetOffset;

        // The draw X position for the new tab button is the rightEdge of the tab strip.
        return rightEdge;
    }

    private float computeNewTabButtonOffsetRtl(StripLayoutTab[] indexOrderedTabs,
            float stripLeftMargin, float stripRightMargin, float stripWidth,
            float newTabButtonWidth, float touchTargetOffset, float cachedTabWidth,
            boolean animate) {
        boolean tabStripImpEnabled = ChromeFeatureList.sTabStripImprovements.isEnabled();

        float leftEdge = stripWidth - stripRightMargin;

        for (StripLayoutTab tab : indexOrderedTabs) {
            float drawX = (tabStripImpEnabled && animate) ? tab.getIdealX() : tab.getDrawX();
            leftEdge = Math.min(drawX, leftEdge);
        }

        leftEdge = Math.max(leftEdge, stripLeftMargin);

        // Adjust the new tab button to be away from the tabs to account for the touch target skew.
        leftEdge -= touchTargetOffset;

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