// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.dynamicmodule;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarCoordinator;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.InflationObserver;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.content_public.common.BrowserControlsState;
import org.chromium.ui.util.TokenHolder;

import javax.inject.Inject;

import dagger.Lazy;

/**
 * Controls the visibility of the toolbar on module managed URLs.
 */
@ActivityScope
public class DynamicModuleToolbarController implements InflationObserver, NativeInitObserver {
    private final Lazy<ChromeFullscreenManager> mFullscreenManager;
    private final CustomTabToolbarCoordinator mToolbarCoordinator;
    private final CustomTabIntentDataProvider mIntentDataProvider;

    private int mControlsHidingToken = TokenHolder.INVALID_TOKEN;
    private boolean mHasReleasedToken;

    @Inject
    public DynamicModuleToolbarController(Lazy<ChromeFullscreenManager> fullscreenManager,
            CustomTabIntentDataProvider intentDataProvider,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            CustomTabToolbarCoordinator toolbarCoordinator) {
        mFullscreenManager = fullscreenManager;
        mToolbarCoordinator = toolbarCoordinator;
        mIntentDataProvider = intentDataProvider;

        activityLifecycleDispatcher.register(this);
    }

    @Override
    public void onPreInflationStartup() {}

    @Override
    public void onPostInflationStartup() {
        mToolbarCoordinator.setBrowserControlsState(BrowserControlsState.HIDDEN);
        mControlsHidingToken =
                mFullscreenManager.get().hideAndroidControlsAndClearOldToken(mControlsHidingToken);
        mHasReleasedToken = false;
    }

    @Override
    public void onFinishNativeInitialization() {
        if (!mIntentDataProvider.isDynamicModuleEnabled()) {
            releaseAndroidControlsHidingToken();
        }
    }

    /* package */ void releaseAndroidControlsHidingToken() {
        mToolbarCoordinator.setBrowserControlsState(BrowserControlsState.BOTH);
        mFullscreenManager.get().releaseAndroidControlsHidingToken(mControlsHidingToken);
        mHasReleasedToken = true;
    }

    @VisibleForTesting
    boolean hasReleasedToken() {
        return mHasReleasedToken;
    }

    @VisibleForTesting
    boolean hasAcquiredToken() {
        return mControlsHidingToken != TokenHolder.INVALID_TOKEN;
    }
}
