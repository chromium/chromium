// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.app.Activity;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Rect;
import android.view.View;

import org.chromium.base.FeatureList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.widget.displaystyle.HorizontalDisplayStyle;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.browser_ui.widget.displaystyle.ViewResizerUtil;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.display.DisplayUtil;

@NullMarked
public class FeedStreamViewResizerUtils {
    // The aspect ratio of large images or video previews, computed based on 1280:720.
    private static final float FEED_IMAGE_OR_VIDEO_ASPECT_RATIO = 1.778f;

    /**
     * Calculates padding for Feed items to ensure visibility in constrained viewports.
     *
     * <p>In landscape mode, the entire large image or video preview cannot fit in the viewport
     * because the floating search bar at the top reduces the user's visible area. To deal with
     * this, we add the left and right paddings to all items in the RecyclerView in order to shrink
     * all card images a little bit so that they can fit in the viewport.
     *
     * @param activity The current activity context.
     * @param uiConfig The UI configuration for display styles.
     * @param view The view to calculate padding for.
     * @param toolbarHeightPx The height of the toolbar to account for in landscape.
     * @return The calculated horizontal padding in pixels.
     */
    public static int computePadding(
            Activity activity, UiConfig uiConfig, View view, int toolbarHeightPx) {

        Resources res = activity.getResources();
        int defaultPaddingPixels = getDefaultPaddingPixels(res);
        int minWidePaddingPixels = getMinWidePaddingPixels(res);

        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(uiConfig.getContext())
                && uiConfig.getCurrentDisplayStyle().isWide()) {
            return computePaddingWide(activity, uiConfig, view, minWidePaddingPixels);
        }
        return computePaddingNarrow(
                activity,
                uiConfig,
                view,
                defaultPaddingPixels,
                toolbarHeightPx,
                minWidePaddingPixels);
    }

    /** Returns the lateral padding to use in {@link HorizontalDisplayStyle#REGULAR}. */
    public static int getDefaultPaddingPixels(Resources resources) {
        return FeatureList.isNativeInitialized()
                        && ChromeFeatureList.isEnabled(ChromeFeatureList.FEED_CONTAINMENT)
                ? resources.getDimensionPixelSize(R.dimen.feed_containment_margin)
                : resources.getDimensionPixelSize(R.dimen.content_suggestions_card_modern_margin);
    }

    /** Returns the minimum lateral padding to use in {@link HorizontalDisplayStyle#WIDE}. */
    public static int getMinWidePaddingPixels(Resources resources) {
        return resources.getDimensionPixelSize(R.dimen.ntp_wide_card_lateral_margins);
    }

    private static int computePaddingNarrow(
            Activity activity,
            UiConfig uiConfig,
            View view,
            int defaultPadding,
            int toolBarHeight,
            int minWidePaddingPixels) {
        int padding =
                ViewResizerUtil.computePadding(
                        view, uiConfig, defaultPadding, minWidePaddingPixels);
        Resources resources = uiConfig.getContext().getResources();
        if (resources.getConfiguration().orientation != Configuration.ORIENTATION_LANDSCAPE
                || activity.isInMultiWindowMode()) {
            return padding;
        }
        float dpToPx = resources.getDisplayMetrics().density;
        float screenWidth = getScreenWidth(uiConfig, view);
        float screenHeight = resources.getConfiguration().screenHeightDp * dpToPx;
        float useableHeight = screenHeight - getStatusBarHeight(activity) - toolBarHeight;
        int customPadding =
                (int) ((screenWidth - useableHeight * FEED_IMAGE_OR_VIDEO_ASPECT_RATIO) / 2);
        return Math.max(customPadding, padding);
    }

    private static int computePaddingWide(
            Activity activity, UiConfig uiConfig, View view, int minWidePaddingPixels) {
        float screenWidth = getScreenWidth(uiConfig, view);
        // (a) Once the width of the body reaches breakpoint,
        // adjust margin sizes while keeping the body width constant.
        Resources resources = activity.getResources();
        int breakpointWidth =
                resources.getDimensionPixelSize(R.dimen.ntp_wide_card_width_breakpoint);
        int customPadding = (int) ((screenWidth - breakpointWidth) / 2);
        // (b) Once the margins reach max, adjust the body size while keeping margins constant.
        customPadding =
                Math.min(
                        customPadding,
                        resources.getDimensionPixelSize(R.dimen.ntp_wide_card_lateral_margins_max));
        // (c) Once the body reaches max width, adjust the margin widths while keeping the body
        // constant.
        int maxWidth = resources.getDimensionPixelSize(R.dimen.ntp_wide_card_width_max);
        customPadding = Math.max(customPadding, (int) (screenWidth - maxWidth) / 2);
        // (d) Return max of computed padding and min allowed margin.
        return Math.max(customPadding, minWidePaddingPixels);
    }

    private static float getScreenWidth(UiConfig uiConfig, View view) {
        Resources resources = uiConfig.getContext().getResources();
        float screenWidth;
        if (DisplayUtil.isUiScaled() && view != null) {
            screenWidth = view.getMeasuredWidth();
        } else {
            float dpToPx = resources.getDisplayMetrics().density;
            screenWidth = resources.getConfiguration().screenWidthDp * dpToPx;
        }
        return screenWidth;
    }

    private static int getStatusBarHeight(Activity activity) {
        Rect rect = new Rect();
        activity.getWindow().getDecorView().getWindowVisibleDisplayFrame(rect);
        return rect.top;
    }

    /**
     * Gets the margin used to compensate for the Feed's containment padding.
     *
     * <p>When containment is enabled on wide displays, a negative margin is returned to counteract
     * the padding applied to the entire NTP. This is to allow all the elements in the NTP header to
     * keep using their existing margins/paddings settings.
     *
     * @param res The {@link Resources} to retrieve dimension pixel sizes.
     * @param uiConfig The {@link UiConfig} to check the current display style.
     * @return The negative compensation margin if applicable, otherwise 0.
     */
    public static int getFeedNtpCompensationMargin(Resources res, UiConfig uiConfig) {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.FEED_CONTAINMENT)
                && uiConfig.getCurrentDisplayStyle().isWide()) {
            return 0;
        }
        int marginValue = res.getDimensionPixelSize(R.dimen.feed_containment_margin);
        return -marginValue;
    }
}
