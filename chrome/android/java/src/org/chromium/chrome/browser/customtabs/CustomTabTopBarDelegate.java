// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.support.annotation.Nullable;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;

/**
 * Delegate that manages top bar area inside of {@link CustomTabActivity}.
 */
class CustomTabTopBarDelegate {
    private ChromeActivity mActivity;
    private ViewGroup mTopBarView;
    @Nullable
    private View mTopBarContentView;

    public CustomTabTopBarDelegate(ChromeActivity activity) {
        mActivity = activity;
    }

    /**
     * Makes the top bar area to show, if any.
     */
    public void showTopBarIfNecessary() {
        if (mTopBarContentView != null && mTopBarContentView.getParent() == null) {
            getTopBarView().addView(mTopBarContentView);
        }
    }

    /**
     * Sets the content of the top bar.
     */
    public void setTopBarContentView(View view) {
        mTopBarContentView = view;
    }

    /**
     * Gets the {@link ViewGroup} of the top bar. If it has not been inflated, inflate it first.
     */
    private ViewGroup getTopBarView() {
        if (mTopBarView == null) {
            ViewStub topBarStub = ((ViewStub) mActivity.findViewById(R.id.topbar_stub));
            mTopBarView = (ViewGroup) topBarStub.inflate();
        }
        return mTopBarView;
    }
}
