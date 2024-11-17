// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import org.chromium.base.supplier.Supplier;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.components.browser_ui.util.BrowserControlsVisibilityDelegate;

/** Implementation of {@link BrowserControlsVisibilityDelegate} for custom tabs. */
public class CustomTabBrowserControlsVisibilityDelegate extends BrowserControlsVisibilityDelegate {
    private final Supplier<BrowserControlsVisibilityManager> mBrowserControlsVisibilityManager;
    private @BrowserControlsState int mBrowserControlsState = BrowserControlsState.BOTH;

    public CustomTabBrowserControlsVisibilityDelegate(
            Supplier<BrowserControlsVisibilityManager> controlsVisibilityManager) {
        super(BrowserControlsState.BOTH);
        mBrowserControlsVisibilityManager = controlsVisibilityManager;
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
