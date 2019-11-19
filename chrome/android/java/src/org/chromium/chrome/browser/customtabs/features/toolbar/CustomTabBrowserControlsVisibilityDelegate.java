// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.fullscreen.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.tab.BrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.tab.TabBrowserControlsState;
import org.chromium.content_public.common.BrowserControlsState;

import javax.inject.Inject;

import dagger.Lazy;

/**
 * Implementation of {@link BrowserControlsVisibilityDelegate} for custom tabs.
 */
@ActivityScope
public class CustomTabBrowserControlsVisibilityDelegate
        implements BrowserControlsVisibilityDelegate {
    private final Lazy<ChromeFullscreenManager> mFullscreenManagerDelegate;
    private final ActivityTabProvider mTabProvider;
    private @BrowserControlsState int mBrowserControlsState = BrowserControlsState.BOTH;

    @Inject
    public CustomTabBrowserControlsVisibilityDelegate(
            Lazy<ChromeFullscreenManager> fullscreenManager, ActivityTabProvider tabProvider) {
        mFullscreenManagerDelegate = fullscreenManager;
        mTabProvider = tabProvider;
    }

    /**
     * Sets the browser controls state. Note: this is not enough to completely hide the toolbar, use
     * {@link CustomTabToolbarCoordinator#setBrowserControlsState()} for that.
     */
    public void setControlsState(@BrowserControlsState int browserControlsState) {
        if (browserControlsState == mBrowserControlsState) return;
        mBrowserControlsState = browserControlsState;
        updateActiveTabFullscreenEnabledState();
    }

    @Override
    public boolean canShowBrowserControls() {
        return mBrowserControlsState != BrowserControlsState.HIDDEN
                && getDefaultVisibilityDelegate().canShowBrowserControls();
    }

    @Override
    public boolean canAutoHideBrowserControls() {
        return mBrowserControlsState != BrowserControlsState.SHOWN
                && getDefaultVisibilityDelegate().canAutoHideBrowserControls();
    }

    private BrowserStateBrowserControlsVisibilityDelegate getDefaultVisibilityDelegate() {
        return mFullscreenManagerDelegate.get().getBrowserVisibilityDelegate();
    }

    private void updateActiveTabFullscreenEnabledState() {
        TabBrowserControlsState.updateEnabledState(mTabProvider.get());
    }
}
