// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.base.LocalizationUtils;

/**
 * A stacker that tells the {@link StripLayoutHelper} how to layer the tabs for the
 * {@link StaticLayout} when the available window width is < 600dp. Tabs will be stacked side by
 * side and the entire strip will scroll. Tabs will never completely overlap each other.
 */
public class ScrollingStripStacker extends StripStacker {
    @Override
    public void setTabOffsets(int selectedIndex, StripLayoutTab[] indexOrderedTabs,
            float tabStackWidth, int maxTabsToStack, float tabOverlapWidth, float stripLeftMargin,
            float stripRightMargin, float stripWidth, boolean inReorderMode, boolean tabClosing,
            float cachedTabWidth) {
        boolean tabStripImpEnabled = ChromeFeatureList.sTabStripImprovements.isEnabled();
        for (int i = 0; i < indexOrderedTabs.length; i++) {
            StripLayoutTab tab = indexOrderedTabs[i];
            if (tabStripImpEnabled) {
                // When a tab is closed, drawX and width update will be animated so skip this.
                if (!tabClosing) {
                    tab.setDrawX(tab.getIdealX() + tab.getOffsetX());
                    tab.setWidth(cachedTabWidth);
                }
            } else {
                tab.setDrawX(tab.getIdealX());
            }
            tab.setDrawY(tab.getOffsetY());
            tab.setVisiblePercentage(1.f);
            tab.setContentOffsetX(0.f);
        }
    }

    @Override
    public float computeNewTabButtonOffset(StripLayoutTab[] indexOrderedTabs, float tabOverlapWidth,
            float stripLeftMargin, float stripRightMargin, float stripWidth, float buttonWidth,
            float touchTargetOffset, float cachedTabWidth, boolean animate) {
        if (ChromeFeatureList.sTabStripImprovements.isEnabled()) {
            return super.computeNewTabButtonOffset(indexOrderedTabs, tabOverlapWidth,
                    stripLeftMargin, stripRightMargin, stripWidth, buttonWidth, touchTargetOffset,
                    cachedTabWidth, animate);
        }
        return LocalizationUtils.isLayoutRtl()
                ? computeNewTabButtonOffsetRtl(indexOrderedTabs, tabOverlapWidth, stripRightMargin,
                        stripWidth, buttonWidth)
                : computeNewTabButtonOffsetLtr(indexOrderedTabs, tabOverlapWidth, stripLeftMargin);
    }

    @Override
    public void performOcclusionPass(int selectedIndex, StripLayoutTab[] indexOrderedTabs,
            float stripWidth) {
        for (int i = 0; i < indexOrderedTabs.length; i++) {
            StripLayoutTab tab = indexOrderedTabs[i];
            tab.setVisible((tab.getDrawX() + tab.getWidth()) >= 0 && tab.getDrawX() <= stripWidth);
        }
    }

    private float computeNewTabButtonOffsetLtr(
            StripLayoutTab[] indexOrderedTabs, float tabOverlapWidth, float stripLeftMargin) {
        float rightEdge = stripLeftMargin;

        int numTabs = indexOrderedTabs.length;
        // Need to look at the last two tabs to determine the new tab position in case the last
        // tab is dying.
        int i = numTabs > 0 ? ((numTabs >= 2) ? numTabs - 2 : numTabs - 1) : 0;
        for (; i < numTabs; i++) {
            StripLayoutTab tab = indexOrderedTabs[i];
            float layoutWidth;
            if (tab.isDying()) {
                // If a tab is dying, adjust the tab width by the width weight so that the new
                // tab button slides to the left with the closing tab.
                layoutWidth = tab.getWidth() * tab.getWidthWeight();
            } else {
                // If a tab is being created, disregard its width weight so the new tab button
                // doesn't end up positioned too far to the left. If a tab is neither being
                // created or dying, its width width weight is 1.0 and can also be ignored.
                layoutWidth = tab.getWidth();
            }

            rightEdge = Math.max(tab.getDrawX() + layoutWidth, rightEdge);
        }

        // Adjust the right edge by the tab overlap width so that the new tab button is nestled
        // closer to the tab.
        rightEdge -= tabOverlapWidth / 2;

        // The draw X position for the new tab button is the rightEdge of the tab strip.
        return rightEdge;
    }

    private float computeNewTabButtonOffsetRtl(StripLayoutTab[] indexOrderedTabs,
            float tabOverlapWidth, float stripRightMargin, float stripWidth,
            float newTabButtonWidth) {
        float leftEdge = stripWidth - stripRightMargin;
        int numTabs = indexOrderedTabs.length;
        if (numTabs >= 1) {
            StripLayoutTab tab = indexOrderedTabs[numTabs - 1];
            leftEdge = Math.min(tab.getDrawX(), leftEdge);
        }

        // Adjust the left edge by the tab overlap width so that the new tab button is nestled
        // closer to the tab.
        leftEdge += tabOverlapWidth / 2;

        // The draw X position for the new tab button is the left edge of the tab strip minus
        // the new tab button width.
        return leftEdge - newTabButtonWidth;
    }
}