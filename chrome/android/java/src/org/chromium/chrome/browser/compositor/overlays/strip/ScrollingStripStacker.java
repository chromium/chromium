// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import org.chromium.ui.base.LocalizationUtils;

/**
 * A stacker that tells the {@link StripLayoutHelper} how to layer the views for the {@link
 * StaticLayout}. Tabs will be stacked side by side and the entire strip will scroll. Tabs will
 * never completely overlap each other.
 */
public class ScrollingStripStacker extends StripStacker {
    @Override
    public void setViewOffsets(
            StripLayoutView[] indexOrderedViews, boolean tabClosing, float cachedTabWidth) {
        for (int i = 0; i < indexOrderedViews.length; i++) {
            StripLayoutView view = indexOrderedViews[i];

            float newDrawX = view.getIdealX() + view.getOffsetX();
            // Adjust the newDrawX to correctly animate container slide-out in RTL.
            // TODO(crbug.com/375029950): Investigate if this is still needed.
            if (LocalizationUtils.isLayoutRtl()
                    && !tabClosing
                    && view instanceof StripLayoutTab tab) {
                newDrawX += (cachedTabWidth - tab.getWidth());
            }
            view.setDrawX(newDrawX);

            // TODO(crbug.com/375030505): Make generic.
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
                float paddedX = groupTitle.getPaddedX();
                float paddedWidth = groupTitle.getPaddedWidth();
                float bottomIndicatorWidth = groupTitle.getBottomIndicatorWidth();

                drawX = paddedX;
                if (LocalizationUtils.isLayoutRtl() && bottomIndicatorWidth > 0) {
                    drawX += paddedWidth - bottomIndicatorWidth;
                }
                width = Math.max(bottomIndicatorWidth, paddedWidth);
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
