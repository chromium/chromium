// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import org.chromium.ui.base.LocalizationUtils;

/**
 * A stacker that tells the {@link StripLayoutHelper} how to layer the views for the {@link
 * StaticLayout} when the available window width is < 600dp. Tabs will be stacked side by side and
 * the entire strip will scroll. Tabs will never completely overlap each other.
 */
public class ScrollingStripStacker extends StripStacker {
    @Override
    public void setViewOffsets(
            StripLayoutView[] indexOrderedViews,
            boolean tabClosing,
            boolean tabCreating,
            boolean groupTitleSlidingAnimRunning,
            boolean groupCollapsingOrExpanding,
            float cachedTabWidth) {
        for (int i = 0; i < indexOrderedViews.length; i++) {
            StripLayoutView view = indexOrderedViews[i];
            // When a tab is closed or group title sliding animation is running, drawX and width
            // update will be animated so skip this.
            if (!tabClosing && !groupTitleSlidingAnimRunning) {
                view.setDrawX(view.getIdealX() + view.getOffsetX());

                if (view instanceof StripLayoutTab tab) {
                    // Properly animate container slide-out in RTL.
                    if (tabCreating && LocalizationUtils.isLayoutRtl()) {
                        tab.setDrawX(tab.getDrawX() + cachedTabWidth - tab.getWidth());
                    }

                    // When a tab is being created, a tab group is collapsing, or a tab group is
                    // expanding, then all tabs are instead animating to their desired width.
                    if (!tabCreating && !tab.isCollapsed() && !groupCollapsingOrExpanding) {
                        tab.setWidth(cachedTabWidth);
                    }
                }
            }

            if (view instanceof StripLayoutTab tab) {
                tab.setDrawY(tab.getOffsetY());
            }
        }
    }

    @Override
    public void performOcclusionPass(
            StripLayoutView[] indexOrderedViews, float xOffset, float visibleWidth) {
        for (int i = 0; i < indexOrderedViews.length; i++) {
            StripLayoutView view = indexOrderedViews[i];
            float drawX;
            float width;
            if (view instanceof StripLayoutGroupTitle groupTitle) {
                drawX = groupTitle.getPaddedX();
                width = groupTitle.getBottomIndicatorWidth();
            } else {
                drawX = view.getDrawX();
                width = view.getWidth();
                if (width < StripLayoutTab.MIN_WIDTH) {
                    // Hide the tab if its width is too small to properly display its favicon.
                    view.setVisible(false);
                    continue;
                }
            }
            view.setVisible((drawX + width) >= xOffset && drawX <= xOffset + visibleWidth);
        }
    }
}
