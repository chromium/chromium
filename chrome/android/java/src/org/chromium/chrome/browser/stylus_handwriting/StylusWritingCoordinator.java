// Copyright 2022 The Chromium Authors
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
import org.chromium.components.stylus_handwriting.StylusHandwritingFeatureMap;
import org.chromium.components.stylus_handwriting.StylusWritingController;
import org.chromium.components.stylus_handwriting.StylusWritingSettingsState;

/**
 * This class coordinates the Tab events and Window focus events required for Stylus handwriting.
 */
public class StylusWritingCoordinator implements WindowFocusChangedObserver {
    private final Activity mActivity;
    private final CurrentTabObserver mCurrentTabObserver;
    private final ObservableSupplier<Tab> mTabProvider;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final StylusWritingController mStylusWritingController;

    public StylusWritingCoordinator(
            Activity activity,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            ObservableSupplier<Tab> activityTabProvider) {
        mActivity = activity;
        mTabProvider = activityTabProvider;
        mStylusWritingController = new StylusWritingController(mActivity.getApplicationContext());
        if (StylusHandwritingFeatureMap.isEnabledOrDefault(
                StylusHandwritingFeatureMap.CACHE_STYLUS_SETTINGS, false)) {
            StylusWritingSettingsState.getInstance().registerObserver(mStylusWritingController);
        }

        lifecycleDispatcher.register(this);
        mLifecycleDispatcher = lifecycleDispatcher;
        mCurrentTabObserver =
                new CurrentTabObserver(
                        activityTabProvider,
                        new EmptyTabObserver() {
                            @Override
                            public void onContentChanged(Tab tab) {
                                if (tab.getWebContents() == null) return;
                                mStylusWritingController.onWebContentsChanged(tab.getWebContents());
                                tab.getContentView()
                                        .setStylusWritingIconSupplier(
                                                mStylusWritingController::resolvePointerIcon);
                            }
                        },
                        /* swap Callback */
                        tab -> {
                            if (tab == null || tab.getWebContents() == null) return;
                            mStylusWritingController.onWebContentsChanged(tab.getWebContents());
                            tab.getContentView()
                                    .setStylusWritingIconSupplier(
                                            mStylusWritingController::resolvePointerIcon);
                        });
    }

    public void destroy() {
        if (StylusHandwritingFeatureMap.isEnabledOrDefault(
                StylusHandwritingFeatureMap.CACHE_STYLUS_SETTINGS, false)) {
            StylusWritingSettingsState.getInstance().unregisterObserver(mStylusWritingController);
        }
        mLifecycleDispatcher.unregister(this);
        mCurrentTabObserver.destroy();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        mStylusWritingController.onWindowFocusChanged(hasFocus);
    }
}
