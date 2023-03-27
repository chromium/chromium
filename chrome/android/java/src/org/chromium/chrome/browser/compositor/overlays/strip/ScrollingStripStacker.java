// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

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
            boolean tabCreating, float cachedTabWidth) {
        for (int i = 0; i < indexOrderedTabs.length; i++) {
            StripLayoutTab tab = indexOrderedTabs[i];
            // When a tab is closed, drawX and width update will be animated so skip this.
            if (!tabClosing) {
                tab.setDrawX(tab.getIdealX() + tab.getOffsetX());
                // When a tab is being created, all tabs are animating to their desired width.
                if (!tabCreating) tab.setWidth(cachedTabWidth);
            }
            tab.setDrawY(tab.getOffsetY());
            tab.setVisiblePercentage(1.f);
            tab.setContentOffsetX(0.f);
        }
    }

    @Override
    public void performOcclusionPass(
            int selectedIndex, StripLayoutTab[] indexOrderedTabs, float stripWidth) {
        for (int i = 0; i < indexOrderedTabs.length; i++) {
            StripLayoutTab tab = indexOrderedTabs[i];
            tab.setVisible((tab.getDrawX() + tab.getWidth()) >= 0 && tab.getDrawX() <= stripWidth);
        }
    }
}