// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.indicator;

import android.app.Activity;
import android.view.View.OnClickListener;
import android.view.ViewGroup;

import androidx.annotation.Nullable;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarView;
import org.chromium.ui.base.WindowAndroid;

/**
 * Visual representation of a snackbar positioned at the top.
 */
public class TopSnackbarView extends SnackbarView {
    private final Activity mActivity;
    private final Supplier<BrowserControlsManager> mBrowserControlsManagerSupplier;

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
            @Nullable WindowAndroid windowAndroid,
            @Nullable Supplier<BrowserControlsManager> browserControlsManagerSupplier) {
        super(activity, listener, snackbar, (ViewGroup) activity.findViewById(android.R.id.content),
                windowAndroid);
        mActivity = activity;
        mBrowserControlsManagerSupplier = browserControlsManagerSupplier;
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
    public void announceforAccessibility() {
        mMessageView.announceForAccessibility(mMessageView.getContentDescription() + " "
                + mContainerView.getResources().getString(R.string.top_bar_screen_position));
    }

    private int getOffsetFromTop() {
        if (mBrowserControlsManagerSupplier == null
                || !mBrowserControlsManagerSupplier.hasValue()) {
            return 0;
        }
        if (mBrowserControlsManagerSupplier.get().getContentOffset() == 0) return 0;

        return mBrowserControlsManagerSupplier.get().getTopControlsHeight();
    }
}
