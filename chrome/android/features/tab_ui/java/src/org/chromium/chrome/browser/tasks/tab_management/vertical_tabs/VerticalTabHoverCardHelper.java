// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import android.content.Context;
import android.util.DisplayMetrics;
import android.view.View;

import androidx.coordinatorlayout.widget.CoordinatorLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tasks.tab_management.TabHoverCardView;

/** Helper for tab hover card operations in vertical tabs. */
@NullMarked
public class VerticalTabHoverCardHelper {

    /** Interface to receive tab hover card events. */
    @FunctionalInterface
    public interface TabHoverCardListener {
        void onTabHoverCardStateChanged(int tabId, View view, boolean isHovered);
    }

    /**
     * Get the x and y coordinates of the position of the hover card, in px.
     *
     * @param tabView The tab item view being hovered.
     * @param containerView The vertical tab container / parent view.
     * @param hoverCardView The hover card view instance.
     * @return A float array specifying the x (array[0]) and y (array[1]) coordinates.
     */
    public static float[] getHoverCardPosition(
            View tabView, View containerView, TabHoverCardView hoverCardView) {
        View root = containerView.getRootView();
        int[] tabViewLocation = new int[2];
        int[] rootLocation = new int[2];
        tabView.getLocationOnScreen(tabViewLocation);
        root.getLocationOnScreen(rootLocation);
        float relativeX = tabViewLocation[0] - rootLocation[0];
        float relativeY = tabViewLocation[1] - rootLocation[1];

        Context context = hoverCardView.getContext();
        float hoverCardWidth = context.getResources().getDimension(R.dimen.tab_hover_card_width);
        DisplayMetrics displayMetrics = context.getResources().getDisplayMetrics();
        float windowWidthPx = displayMetrics.widthPixels;
        hoverCardWidth =
                Math.min(
                        hoverCardWidth,
                        TabHoverCardView.HOVER_CARD_MAX_WIDTH_PERCENT * windowWidthPx);

        var layoutParams = hoverCardView.getLayoutParams();
        if (layoutParams != null && hoverCardWidth != layoutParams.width) {
            hoverCardView.setLayoutParams(
                    new CoordinatorLayout.LayoutParams(
                            Math.round(hoverCardWidth), layoutParams.height));
        }

        float hoverCardX = relativeX + tabView.getWidth();
        float hoverCardY = relativeY;

        hoverCardView.measure(
                View.MeasureSpec.makeMeasureSpec(
                        Math.round(hoverCardWidth), View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED));
        float hoverCardHeight = hoverCardView.getMeasuredHeight();

        float parentHeight = root.getHeight();
        if (hoverCardY + hoverCardHeight > parentHeight) {
            hoverCardY = parentHeight - hoverCardHeight;
        }
        if (hoverCardY < 0) {
            hoverCardY = 0;
        }
        return new float[] {hoverCardX, hoverCardY};
    }
}
