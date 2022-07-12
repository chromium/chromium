// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.stylus_handwriting;

import android.app.Activity;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.WindowFocusChangedObserver;
import org.chromium.chrome.browser.tab.CurrentTabObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.stylus_handwriting.ApiHelperForStylusWriting;

/**
 * This class coordinates the Tab events and Window focus events required for Stylus handwriting.
 */
public class StylusWritingCoordinator implements WindowFocusChangedObserver {
    private final Activity mActivity;
    private final CurrentTabObserver mCurrentTabObserver;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;

    public StylusWritingCoordinator(Activity activity,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            ObservableSupplier<Tab> activityTabProvider) {
        lifecycleDispatcher.register(this);
        mLifecycleDispatcher = lifecycleDispatcher;
        mCurrentTabObserver = new CurrentTabObserver(activityTabProvider,
                new EmptyTabObserver() {
                    @Override
                    public void onContentChanged(Tab tab) {
                        if (tab.getWebContents() == null) return;
                        ApiHelperForStylusWriting.onWebContentsInitialized(tab.getWebContents());
                    }
                },
                /* swap Callback */
                tab -> {
                    if (tab == null || tab.getWebContents() == null) return;
                    ApiHelperForStylusWriting.onWebContentsInitialized(tab.getWebContents());
                });
        mActivity = activity;
    }

    public void destroy() {
        mLifecycleDispatcher.unregister(this);
        mCurrentTabObserver.destroy();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        ApiHelperForStylusWriting.onWindowFocusChanged(hasFocus, mActivity);
    }
}
