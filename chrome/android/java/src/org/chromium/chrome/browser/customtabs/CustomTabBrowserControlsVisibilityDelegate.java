// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.tab.BrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.tab.Tab;

import javax.inject.Inject;

/**
 * Implementation of {@link BrowserControlsVisibilityDelegate} for custom tabs.
 */
@ActivityScope
public class CustomTabBrowserControlsVisibilityDelegate
        implements BrowserControlsVisibilityDelegate {
    private final BrowserControlsVisibilityDelegate mFullscreenManagerDelegate;
    private final ActivityTabProvider mTabProvider;
    private boolean mIsInTwaMode;

    @Inject
    public CustomTabBrowserControlsVisibilityDelegate(
            ChromeFullscreenManager fullscreenManager, ActivityTabProvider tabProvider) {
        mFullscreenManagerDelegate = fullscreenManager.getBrowserVisibilityDelegate();
        mTabProvider = tabProvider;
    }

    /**
     * Sets trusted web activity mode. In trusted web activity mode browser controls should be
     * hidden.
     */
    public void setTrustedWebActivityMode(boolean isInTwaMode) {
        if (mIsInTwaMode == isInTwaMode) {
            return;
        }
        mIsInTwaMode = isInTwaMode;
        Tab activeTab = mTabProvider.getActivityTab();
        if (activeTab != null) {
            activeTab.updateFullscreenEnabledState();
        }
    }

    @Override
    public boolean canShowBrowserControls() {
        return !mIsInTwaMode && mFullscreenManagerDelegate.canShowBrowserControls();
    }

    @Override
    public boolean canAutoHideBrowserControls() {
        return mFullscreenManagerDelegate.canAutoHideBrowserControls();
    }
}
