// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import org.chromium.base.MathUtils;
import org.chromium.ui.base.LocalizationUtils;

/**
 * A stacker that tells the {@link StripLayoutHelper} how to layer the tabs for the
 * {@link StaticLayout} when the available window width is >= 600dp. Tabs will be stacked with the
 * focused tab in front with tabs cascading back to each side.
 */
public class CascadingStripStacker extends StripStacker {
    @Override
    public void setTabOffsets(int selectedIndex, StripLayoutTab[] indexOrderedTabs,
            float tabStackWidth, int maxTabsToStack, float tabOverlapWidth, float stripLeftMargin,
            float stripRightMargin, float stripWidth, boolean inReorderMode, boolean tabClosing,
            boolean tabCreating, float cachedTabWidth) {
        // 1. Calculate the size of the selected tab.  This is used later to figure out how
        // occluded the tabs are.
        final StripLayoutTab selTab = selectedIndex >= 0 ? indexOrderedTabs[selectedIndex] : null;
        final float selTabWidth = selTab != null ? selTab.getWidth() : 0;
        final float selTabVisibleSize = selTabWidth - tabStackWidth - tabOverlapWidth;

        for (int i = 0; i < indexOrderedTabs.length; i++) {
            StripLayoutTab tab = indexOrderedTabs[i];

            float posX = tab.getIdealX();

            // 2. Calculate how many tabs are stacked on the left or the right, giving us an idea
            // of where we can stack this current tab.
            int leftStackCount = (i < selectedIndex) ? Math.min(i, maxTabsToStack)
                                                : Math.min(maxTabsToStack, selectedIndex)
                            + Math.min(maxTabsToStack, i - selectedIndex);

            int rightStackCount = (i >= selectedIndex)
                    ? Math.min(indexOrderedTabs.length - 1 - i, maxTabsToStack)
                    : Math.min(indexOrderedTabs.length - 1 - selectedIndex, maxTabsToStack)
                            + Math.min(selectedIndex - i, maxTabsToStack);

            if (LocalizationUtils.isLayoutRtl()) {
                int oldLeft = leftStackCount;
                leftStackCount = rightStackCount;
                rightStackCount = oldLeft;
            }

            // 3. Calculate the proper draw position for the tab.  Clamp based on stacking
            // rules.
            float minDrawX = tabStackWidth * leftStackCount + stripLeftMargin;
            float maxDrawX = stripWidth - tabStackWidth * rightStackCount - stripRightMargin;

            float drawX =
                    MathUtils.clamp(posX + tab.getOffsetX(), minDrawX, maxDrawX - tab.getWidth());

            // TODO(dtrainor): Don't set drawX if the tab is closing?
            tab.setDrawX(drawX);
            tab.setDrawY(tab.getOffsetY());

            // 4. Calculate how visible this tab is.
            float visiblePercentage = 1.f;
            if (i != selectedIndex) {
                final float effectiveTabWidth = Math.max(tab.getWidth(), 1.f);
                final boolean leftStack =
                        LocalizationUtils.isLayoutRtl() ? i > selectedIndex : i < selectedIndex;
                final float minVisible = !leftStack ? minDrawX + selTabVisibleSize : minDrawX;
                final float maxVisible = leftStack ? maxDrawX - selTabVisibleSize : maxDrawX;

                final float clippedTabWidth =
                        Math.min(posX + effectiveTabWidth, maxVisible) - Math.max(posX, minVisible);
                visiblePercentage = MathUtils.clamp(clippedTabWidth / effectiveTabWidth, 0.f, 1.f);
            }
            tab.setVisiblePercentage(visiblePercentage);

            // 5. Calculate which index we start sliding content for.
            // When reordering, we don't want to slide the content of the adjacent tabs.
            int contentOffsetIndex = inReorderMode ? selectedIndex + 1 : selectedIndex;

            // 6. Calculate how much the tab is overlapped on the left side or right for RTL.
            float hiddenAmount = 0.f;
            if (i > contentOffsetIndex && i > 0) {
                // 6.a. Get the effective right edge of the previous tab.
                final StripLayoutTab prevTab = indexOrderedTabs[i - 1];
                final float prevLayoutWidth =
                        (prevTab.getWidth() - tabOverlapWidth) * prevTab.getWidthWeight();
                float prevTabRight = prevTab.getDrawX();
                if (!LocalizationUtils.isLayoutRtl()) prevTabRight += prevLayoutWidth;

                // 6.b. Subtract our current draw X from the previous tab's right edge and
                // get the percentage covered.
                hiddenAmount = Math.max(prevTabRight - drawX, 0);
                if (LocalizationUtils.isLayoutRtl()) {
                    // Invert The amount because we're RTL.
                    hiddenAmount = prevLayoutWidth - hiddenAmount;
                }
            }

            tab.setContentOffsetX(hiddenAmount);
        }
    }

    @Override
    public void performOcclusionPass(int selectedIndex, StripLayoutTab[] indexOrderedTabs,
            float stripWidth) {
        for (int i = 1; i < indexOrderedTabs.length; i++) {
            StripLayoutTab prevTab = indexOrderedTabs[i - 1];
            StripLayoutTab currTab = indexOrderedTabs[i];

            if ((int) prevTab.getDrawY() == (int) currTab.getDrawY()
                    && (int) prevTab.getDrawX() == (int) currTab.getDrawX()) {
                if (i <= selectedIndex) {
                    prevTab.setVisible(false);
                } else if (i > selectedIndex) {
                    currTab.setVisible(false);
                }
            } else if ((int) prevTab.getDrawX() != (int) currTab.getDrawX()) {
                if (i <= selectedIndex) {
                    prevTab.setVisible(true);
                } else if (i > selectedIndex) {
                    currTab.setVisible(true);
                }
            }

            if (i == selectedIndex) currTab.setVisible(true);

            // If index 0 is selected, this line is required to set its visibility correctly.
            if (i - 1 == selectedIndex) prevTab.setVisible(true);
        }
    }
}