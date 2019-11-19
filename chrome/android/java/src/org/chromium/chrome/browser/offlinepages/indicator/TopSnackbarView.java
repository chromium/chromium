// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.indicator;

import android.app.Activity;
import android.view.View.OnClickListener;
import android.view.ViewGroup;

import androidx.annotation.Nullable;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.snackbar.Snackbar;
import org.chromium.chrome.browser.snackbar.SnackbarView;

/**
 * Visual representation of a snackbar positioned at the top.
 */
public class TopSnackbarView extends SnackbarView {
    /**
     * Creates an instance of the {@link SnackbarView}.
     * @param activity The activity that displays the snackbar.
     * @param listener An {@link OnClickListener} that will be called when the action button is
     *                 clicked.
     * @param snackbar The snackbar to be displayed.
     * @param parentView The ViewGroup used to display this snackbar. If this is null, this class
     *                   will determine where to attach the snackbar.
     */
    public TopSnackbarView(Activity activity, OnClickListener listener, Snackbar snackbar,
            @Nullable ViewGroup parentView) {
        super(activity, listener, snackbar, parentView);
    }

    @Override
    protected int getYPositionForMoveAnimation() {
        return -mContainerView.getHeight();
    }

    @Override
    protected int getBottomMarginForLayout() {
        return mParent.getHeight() - mSnackbarView.getHeight() - getOffsetFromTop();
    }

    @Override
    protected ViewGroup findParentView(Activity activity) {
        // Override this in order not to associate top snackbar view with bottom container view.
        return (ViewGroup) activity.findViewById(android.R.id.content);
    }

    @Override
    public void announceforAccessibility() {
        mMessageView.announceForAccessibility(mMessageView.getContentDescription() + " "
                + mContainerView.getResources().getString(R.string.top_bar_screen_position));
    }

    private int getOffsetFromTop() {
        if (!(mActivity instanceof ChromeActivity)) return 0;

        ChromeActivity chromeActivity = (ChromeActivity) mActivity;

        if (chromeActivity.getFullscreenManager().getContentOffset() == 0) return 0;

        return chromeActivity.getFullscreenManager().getTopControlsHeight();
    }
}
