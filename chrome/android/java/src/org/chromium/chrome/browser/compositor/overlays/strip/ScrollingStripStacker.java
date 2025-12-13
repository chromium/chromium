// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.base.LocalizationUtils;

/**
 * A stacker that tells the {@link StripLayoutHelper} how to layer the views for tab strip. Tabs
 * will be stacked side by side and the entire strip will scroll. Tabs will never completely overlap
 * each other.
 */
@NullMarked
public class ScrollingStripStacker extends StripStacker {

    @Override
    public void pushDrawPropertiesToViews(
            StripLayoutView[] indexOrderedViews, float leftBound, float rightBound) {
        for (int i = 0; i < indexOrderedViews.length; i++) {
            StripLayoutView view = indexOrderedViews[i];

            view.setDrawX(view.getIdealX() + view.getOffsetX());
            view.setDrawY(view.getOffsetY());
            // visibility is based drawX - call this after setting drawX / Y.
            setVisible(view, leftBound, rightBound);
        }
    }

    private static void setVisible(StripLayoutView view, float leftBound, float rightBound) {
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
                (drawXAccountingPadding + width) >= leftBound
                        && drawXAccountingPadding <= rightBound);
    }
}
