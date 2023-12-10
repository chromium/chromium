// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import dagger.Lazy;

import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.components.browser_ui.util.BrowserControlsVisibilityDelegate;

import javax.inject.Inject;

/** Implementation of {@link BrowserControlsVisibilityDelegate} for custom tabs. */
@ActivityScope
public class CustomTabBrowserControlsVisibilityDelegate extends BrowserControlsVisibilityDelegate {
    private final Lazy<BrowserControlsVisibilityManager> mBrowserControlsVisibilityManager;
    private final ActivityTabProvider mTabProvider;
    private @BrowserControlsState int mBrowserControlsState = BrowserControlsState.BOTH;

    @Inject
    public CustomTabBrowserControlsVisibilityDelegate(
            Lazy<BrowserControlsVisibilityManager> controlsVisibilityManager,
            ActivityTabProvider tabProvider) {
        super(BrowserControlsState.BOTH);
        mBrowserControlsVisibilityManager = controlsVisibilityManager;
        mTabProvider = tabProvider;
        getDefaultVisibilityDelegate().addObserver((constraints) -> updateVisibilityConstraints());
        updateVisibilityConstraints();
    }

    /**
     * Sets the browser controls state. Note: this is not enough to completely hide the toolbar, use
     * {@link CustomTabToolbarCoordinator#setBrowserControlsState()} for that.
     */
    public void setControlsState(@BrowserControlsState int browserControlsState) {
        if (browserControlsState == mBrowserControlsState) return;
        mBrowserControlsState = browserControlsState;
        updateVisibilityConstraints();
    }

    private @BrowserControlsState int calculateVisibilityConstraints() {
        @BrowserControlsState int defaultConstraints = getDefaultVisibilityDelegate().get();
        if (defaultConstraints == BrowserControlsState.HIDDEN
                || mBrowserControlsState == BrowserControlsState.HIDDEN) {
            return BrowserControlsState.HIDDEN;
        } else if (mBrowserControlsState != BrowserControlsState.BOTH) {
            return mBrowserControlsState;
        }
        return defaultConstraints;
    }

    private void updateVisibilityConstraints() {
        set(calculateVisibilityConstraints());
    }

    private BrowserStateBrowserControlsVisibilityDelegate getDefaultVisibilityDelegate() {
        return mBrowserControlsVisibilityManager.get().getBrowserVisibilityDelegate();
    }
}
