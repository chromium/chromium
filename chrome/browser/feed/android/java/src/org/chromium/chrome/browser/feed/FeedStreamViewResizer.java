// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.app.Activity;
import android.content.res.Resources;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.widget.displaystyle.HorizontalDisplayStyle;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.browser_ui.widget.displaystyle.ViewResizer;

/**
 * Updates the paddings used to display the feed stream when switching to landscape mode. Due to the
 * fact that the search bar is floating at the top, the entire feed stream needs to shrink a little
 * bit in order to have large image or video preview fit in the viewport.
 */
@NullMarked
public class FeedStreamViewResizer extends ViewResizer {
    private final View mView;
    private final Activity mActivity;
    private final int mToolbarHeightPx;

    /**
     * @param activity The activity displays the view.
     * @param view The view that will have its padding resized.
     * @param config The UiConfig object to subscribe to.
     * @param defaultPaddingPixels Padding to use in {@link HorizontalDisplayStyle#REGULAR}.
     * @param minWidePaddingPixels Minimum lateral padding to use in {@link
     *     HorizontalDisplayStyle#WIDE}.
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

        mToolbarHeightPx =
                mActivity.getResources().getDimensionPixelSize(R.dimen.default_action_bar_height);
    }

    /**
     * Convenience method to create a new ViewResizer and immediately attach it to a {@link
     * UiConfig}. If the {@link UiConfig} can outlive the view, the regular constructor should be
     * used, so it can be detached to avoid memory leaks.
     *
     * @param activity The activity displays the view.
     * @param view The view that will have its padding resized.
     * @param config The UiConfig object to subscribe to.
     * @return The {@link ViewResizer} that is created and attached.
     */
    public static FeedStreamViewResizer createAndAttach(
            Activity activity, View view, UiConfig config) {
        Resources resources = activity.getResources();
        int defaultPaddingPixels = FeedStreamViewResizerUtils.getDefaultPaddingPixels(resources);
        int minWidePaddingPixels = FeedStreamViewResizerUtils.getMinWidePaddingPixels(resources);

        FeedStreamViewResizer viewResizer =
                new FeedStreamViewResizer(
                        activity, view, config, defaultPaddingPixels, minWidePaddingPixels);
        viewResizer.attach();
        return viewResizer;
    }

    /**
     * Calculates padding using {@link FeedStreamViewResizerUtils} to ensure card content fits the
     * viewport, particularly in landscape mode.
     */
    @Override
    protected int computePadding() {
        // Any changes to this feed-specific padding calculation should be implemented
        // in FeedStreamViewResizerUtils. This logic is used for Feed-related layout
        // adjustments by multiple classes beyond FeedStreamViewResizer.
        return FeedStreamViewResizerUtils.computePadding(
                mActivity, mUiConfig, mView, mToolbarHeightPx);
    }
}
