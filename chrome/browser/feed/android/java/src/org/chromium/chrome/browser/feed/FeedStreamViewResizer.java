// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.app.Activity;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Rect;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.BuildInfo;
import org.chromium.base.FeatureList;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.widget.displaystyle.HorizontalDisplayStyle;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.browser_ui.widget.displaystyle.ViewResizer;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * Updates the paddings used to display the feed stream when switching to landscape mode. Due to the
 * fact that the search bar is floating at the top, the entire feed stream needs to shrink a little
 * bit in order to have large image or video preview fit in the viewport.
 */
public class FeedStreamViewResizer extends ViewResizer {
    // The aspect ratio of large images or video previews, computed based on 1280:720.
    private static final float FEED_IMAGE_OR_VIDEO_ASPECT_RATIO = 1.778f;

    private final View mView;
    private final Activity mActivity;

    /**
     * @param activity The activity displays the view.
     * @param view The view that will have its padding resized.
     * @param config The UiConfig object to subscribe to.
     * @param defaultPaddingPixels Padding to use in {@link HorizontalDisplayStyle#REGULAR}.
     * @param minWidePaddingPixels Minimum lateral padding to use in {@link
     *         HorizontalDisplayStyle#WIDE}.
     */
    public FeedStreamViewResizer(
            Activity activity,
            View view,
            UiConfig config,
            int defaultPaddingPixels,
            int minWidePaddingPixels) {
        super(view, config, defaultPaddingPixels, minWidePaddingPixels);
        mView = view;
        mActivity = activity;
    }

    /**
     * Convenience method to create a new ViewResizer and immediately attach it to a {@link
     * UiConfig}. If the {@link UiConfig} can outlive the view, the regular constructor should be
     * used, so it can be detached to avoid memory leaks.
     * @param activity The activity displays the view.
     * @param view The view that will have its padding resized.
     * @param config The UiConfig object to subscribe to.
     * @return The {@link ViewResizer} that is created and attached.
     */
    public static FeedStreamViewResizer createAndAttach(
            Activity activity, View view, UiConfig config) {
        Resources resources = activity.getResources();
        int defaultPaddingPixels =
                FeatureList.isNativeInitialized()
                                && ChromeFeatureList.isEnabled(ChromeFeatureList.FEED_CONTAINMENT)
                        ? resources.getDimensionPixelSize(R.dimen.feed_containment_margin)
                        : resources.getDimensionPixelSize(
                                R.dimen.content_suggestions_card_modern_margin);
        int minWidePaddingPixels =
                resources.getDimensionPixelSize(R.dimen.ntp_wide_card_lateral_margins);

        FeedStreamViewResizer viewResizer =
                new FeedStreamViewResizer(
                        activity, view, config, defaultPaddingPixels, minWidePaddingPixels);
        viewResizer.attach();
        return viewResizer;
    }

    /**
     * In landscape mode, the entire large image or video preview cannot fit in the viewport because
     * the floating search bar at the top reduces the user's visible area. To deal with this, we
     * add the left and right paddings to all items in the RecyclerView in order to shrink all card
     * images a little bit so that they can fit in the viewport.
     */
    @Override
    protected int computePadding() {
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(mUiConfig.getContext())
                && mUiConfig.getCurrentDisplayStyle().isWide()) {
            return computePaddingWide();
        } else {
            return computePaddingNarrow();
        }
    }

    private int computePaddingNarrow() {
        int padding = super.computePadding();
        Resources resources = mUiConfig.getContext().getResources();
        if (resources.getConfiguration().orientation != Configuration.ORIENTATION_LANDSCAPE
                || mActivity.isInMultiWindowMode()) {
            return padding;
        }
        float dpToPx = resources.getDisplayMetrics().density;
        float screenWidth = getScreenWidth();
        float screenHeight = resources.getConfiguration().screenHeightDp * dpToPx;
        float useableHeight = screenHeight - statusBarHeight() - toolbarHeight();
        int customPadding =
                (int) ((screenWidth - useableHeight * FEED_IMAGE_OR_VIDEO_ASPECT_RATIO) / 2);
        return Math.max(customPadding, padding);
    }

    private int computePaddingWide() {
        float screenWidth = getScreenWidth();
        // (a) Once the width of the body reaches breakpoint,
        // adjust margin sizes while keeping the body width constant.
        Resources resources = mActivity.getResources();
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
        return Math.max(customPadding, getMinWidePaddingPixels());
    }

    private float getScreenWidth() {
        Resources resources = mUiConfig.getContext().getResources();
        float screenWidth;
        if (BuildInfo.getInstance().isAutomotive && mView != null) {
            screenWidth = mView.getMeasuredWidth();
        } else {
            float dpToPx = resources.getDisplayMetrics().density;
            screenWidth = resources.getConfiguration().screenWidthDp * dpToPx;
        }
        return screenWidth;
    }

    private int toolbarHeight() {
        ViewGroup contentContainer = mActivity.findViewById(android.R.id.content);
        if (contentContainer == null) {
            return 0;
        }
        View toolbarView = contentContainer.findViewById(R.id.toolbar_container);
        if (toolbarView == null) {
            return 0;
        }
        return toolbarView.getHeight();
    }

    private int statusBarHeight() {
        Rect visibleContentRect = new Rect();
        mActivity.getWindow().getDecorView().getWindowVisibleDisplayFrame(visibleContentRect);
        return visibleContentRect.top;
    }
}
