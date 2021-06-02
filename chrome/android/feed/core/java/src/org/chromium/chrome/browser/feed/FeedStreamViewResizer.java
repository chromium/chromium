// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.app.Activity;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Rect;
import android.view.View;
import android.view.ViewGroup;

import androidx.core.view.ViewCompat;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.browser_ui.widget.displaystyle.ViewResizer;

/**
 * Updates the paddings used to display the feed stream when switching to landscape mode. Due to the
 * fact that the search bar is floating at the top, the entire feed stream needs to shrink a little
 * bit in order to have large image or video preview fit in the viewport.
 */
public class FeedStreamViewResizer extends ViewResizer {
    // The aspect ratio of large images or video previews, computed based on 1280:720.
    private static final float FEED_IMAGE_OR_VIDEO_ASPECT_RATIO = 1.778f;

    private final Activity mActivity;

    /**
     * @param activity The activity displays the view.
     * @param view The view that will have its padding resized.
     * @param config The UiConfig object to subscribe to.
     * @param defaultPaddingPixels Padding to use in {@link HorizontalDisplayStyle#REGULAR}.
     * @param minWidePaddingPixels Minimum lateral padding to use in {@link
     *         HorizontalDisplayStyle#WIDE}.
     */
    public FeedStreamViewResizer(Activity activity, View view, UiConfig config,
            int defaultPaddingPixels, int minWidePaddingPixels) {
        super(view, config, defaultPaddingPixels, minWidePaddingPixels);
        mActivity = activity;
    }

    /**
     * Convenience method to create a new ViewResizer and immediately attach it to a {@link
     * UiConfig}. If the {@link UiConfig} can outlive the view, the regular constructor should be
     * used, so it can be detached to avoid memory leaks.
     * @param activity The activity displays the view.
     * @param view The view that will have its padding resized.
     * @param config The UiConfig object to subscribe to.
     * @param defaultPaddingPixels Padding to use in {@link HorizontalDisplayStyle#REGULAR}.
     * @param minWidePaddingPixels Minimum lateral padding to use in {@link
     *         HorizontalDisplayStyle#WIDE}.
     * @return The {@link ViewResizer} that is created and attached.
     */
    public static FeedStreamViewResizer createAndAttach(Activity activity, View view,
            UiConfig config, int defaultPaddingPixels, int minWidePaddingPixels) {
        FeedStreamViewResizer viewResizer = new FeedStreamViewResizer(
                activity, view, config, defaultPaddingPixels, minWidePaddingPixels);
        viewResizer.attach();
        return viewResizer;
    }

    @Override
    public void onDisplayStyleChanged(UiConfig.DisplayStyle newDisplayStyle) {
        if (mUiConfig.getContext().getResources().getConfiguration().orientation
                        != Configuration.ORIENTATION_LANDSCAPE
                || MultiWindowUtils.getInstance().isInMultiWindowMode(mActivity)) {
            super.onDisplayStyleChanged(newDisplayStyle);
            return;
        }

        updatePaddingForLandscapeMode();
    }

    /**
     * In landscape mode, the entire large image or video preview cannot fit in the viewport because
     * the floating search bar at the top reduces the user's visible area. To deal with this, we
     * add the left and right paddings to all items in the RecyclerView in order to shrink all card
     * images a little bit so that they can fit in the viewport.
     */
    private void updatePaddingForLandscapeMode() {
        ViewGroup contentContainer = mActivity.findViewById(android.R.id.content);
        if (contentContainer == null) {
            return;
        }
        View toolbarView = contentContainer.findViewById(R.id.toolbar_container);
        if (toolbarView == null) {
            return;
        }
        int toolbarHeight = toolbarView.getHeight();

        Resources resources = mUiConfig.getContext().getResources();
        float dpToPx = resources.getDisplayMetrics().density;
        float screenWidth = resources.getConfiguration().screenWidthDp * dpToPx;
        float screenHeight = resources.getConfiguration().screenHeightDp * dpToPx;

        float useableHeight = screenHeight - statusBarHeight() - toolbarHeight;
        int padding = (int) ((screenWidth - useableHeight * FEED_IMAGE_OR_VIDEO_ASPECT_RATIO) / 2);

        ViewCompat.setPaddingRelative(
                mView, padding, mView.getPaddingTop(), padding, mView.getPaddingBottom());
    }

    private int statusBarHeight() {
        Rect visibleContentRect = new Rect();
        mActivity.getWindow().getDecorView().getWindowVisibleDisplayFrame(visibleContentRect);
        return visibleContentRect.top;
    }
}
