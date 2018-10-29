// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.dynamicmodule;

import android.net.Uri;
import android.view.View;

import org.chromium.chrome.browser.customtabs.CustomTabActivity;

/**
 * The implementation of {@link IActivityHost}.
 */
public class ActivityHostImpl extends BaseActivityHost {
    private final CustomTabActivity mActivity;

    public ActivityHostImpl(CustomTabActivity activity) {
        mActivity = activity;
    }

    @Override
    public IObjectWrapper getActivityContext() {
        return ObjectWrapper.wrap(mActivity);
    }

    @Override
    public void setBottomBarView(IObjectWrapper bottomBarView) {
        mActivity.setBottomBarContentView(ObjectWrapper.unwrap(bottomBarView, View.class));
    }

    @Override
    public void setOverlayView(IObjectWrapper overlayView) {
        mActivity.setOverlayView(ObjectWrapper.unwrap(overlayView, View.class));
    }

    @Override
    public void setBottomBarHeight(int height) {
        mActivity.setBottomBarHeight(height);
    }

    @Override
    public void loadUri(Uri uri) {
        mActivity.loadUri(uri);
    }

    @Override
    public void setTopBarView(IObjectWrapper topBarView) {
        mActivity.setTopBarContentView(ObjectWrapper.unwrap(topBarView, View.class));
    }
}
