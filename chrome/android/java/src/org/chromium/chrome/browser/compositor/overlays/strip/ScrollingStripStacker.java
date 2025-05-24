// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import org.chromium.ui.base.LocalizationUtils;

/**
 * A stacker that tells the {@link StripLayoutHelper} how to layer the views for tab strip. Tabs
 * will be stacked side by side and the entire strip will scroll. Tabs will never completely overlap
 * each other.
 */
public class ScrollingStripStacker extends StripStacker {

    @Override
    public void pushDrawPropertiesToViews(
            StripLayoutView[] indexOrderedViews,
            float xOffset,
            float visibleWidth,
            boolean tabClosing,
            float cachedTabWidth) {
        for (int i = 0; i < indexOrderedViews.length; i++) {
            StripLayoutView view = indexOrderedViews[i];

            setDrawXAndY(view, tabClosing, cachedTabWidth);
            // visibility is based drawX - call this after setting drawX / Y.
            setVisible(view, xOffset, visibleWidth);
        }
    }

    private static void setVisible(StripLayoutView view, float xOffset, float visibleWidth) {
        float drawXAccountingPadding = 0f;
        float width = 0f;
        if (view instanceof StripLayoutGroupTitle groupTitle) {
            float paddedX = groupTitle.getPaddedX();
            float paddedWidth = groupTitle.getPaddedWidth();
            float bottomIndicatorWidth = groupTitle.getBottomIndicatorWidth();

            drawXAccountingPadding = paddedX;
            if (LocalizationUtils.isLayoutRtl() && bottomIndicatorWidth > 0) {
                drawXAccountingPadding += paddedWidth - bottomIndicatorWidth;
            }
            width = Math.max(bottomIndicatorWidth, paddedWidth);
        } else if (view instanceof StripLayoutTab) {
            // Tabs do not have padding.
            drawXAccountingPadding = view.getDrawX();
            width = view.getWidth();
            if (width < StripLayoutTab.MIN_WIDTH) {
                // Hide the tab if its width is too small to properly display its favicon.
                view.setVisible(false);
                return;
            }
        } else {
            assert false : "Method should be invoked only for tabs and groups";
        }
        view.setVisible(
                (drawXAccountingPadding + width) >= xOffset
                        && drawXAccountingPadding <= xOffset + visibleWidth);
    }

    private static void setDrawXAndY(
            StripLayoutView view, boolean tabClosing, float cachedTabWidth) {
        float newDrawX = view.getIdealX() + view.getOffsetX();
        // Adjust the newDrawX to correctly animate container slide-out in RTL.
        // TODO(crbug.com/375029950): Investigate if this is still needed.
        if (view instanceof StripLayoutTab tab && LocalizationUtils.isLayoutRtl() && !tabClosing) {
            newDrawX += (cachedTabWidth - tab.getWidth());
        }
        view.setDrawX(newDrawX);
        view.setDrawY(view.getOffsetY());
    }
}
