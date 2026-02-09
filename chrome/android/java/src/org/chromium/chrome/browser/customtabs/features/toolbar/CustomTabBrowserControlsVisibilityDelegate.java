// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.build.annotations.NullMarked;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.components.browser_ui.util.BrowserControlsVisibilityDelegate;

import java.util.function.Supplier;

/**
 * Implementation of {@link BrowserControlsVisibilityDelegate} for custom tabs specific logic. It is
 * a wrapper around the delegate provided by {@link BrowserControlsVisibilityManager}, allow setting
 * constraint without changing the browser state logic (e.g. hide controls in TWA app mode).
 */
@NullMarked
public class CustomTabBrowserControlsVisibilityDelegate extends BrowserControlsVisibilityDelegate {
    private final Supplier<BrowserControlsVisibilityManager> mBrowserControlsVisibilityManager;
    private @BrowserControlsState int mBrowserControlsState = BrowserControlsState.BOTH;

    public CustomTabBrowserControlsVisibilityDelegate(
            Supplier<BrowserControlsVisibilityManager> controlsVisibilityManager) {
        mBrowserControlsVisibilityManager = controlsVisibilityManager;
        getDefaultVisibilityDelegate()
                .addSyncObserverAndPostIfNonNull((constraints) -> updateVisibilityConstraints());
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
        @BrowserControlsState
        int defaultConstraints = assumeNonNull(getDefaultVisibilityDelegate().get());
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
