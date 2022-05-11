// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.components.autofill_assistant.AssistantBrowserControls;

/**
 * Implementation of {@link AssistantBrowserControls} for Chrome.
 */
public class AssistantBrowserControlsChrome
        implements AssistantBrowserControls, BrowserControlsStateProvider.Observer {
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private AssistantBrowserControls.Observer mDelegateObserver;

    public AssistantBrowserControlsChrome(
            BrowserControlsStateProvider browserControlsStateProvider) {
        mBrowserControlsStateProvider = browserControlsStateProvider;
    }

    @Override
    public int getBottomControlsHeight() {
        return mBrowserControlsStateProvider.getBottomControlsHeight();
    }

    @Override
    public int getBottomControlOffset() {
        return mBrowserControlsStateProvider.getBottomControlOffset();
    }

    @Override
    public int getContentOffset() {
        return mBrowserControlsStateProvider.getContentOffset();
    }

    @Override
    public float getTopVisibleContentOffset() {
        return mBrowserControlsStateProvider.getTopVisibleContentOffset();
    }

    @Override
    public void setObserver(AssistantBrowserControls.Observer browserControlsObserver) {
        // Make sure we unregister before registering again. With the current implementation of
        // ObserverList this is not necessary.
        mBrowserControlsStateProvider.removeObserver(this);

        mDelegateObserver = browserControlsObserver;
        mBrowserControlsStateProvider.addObserver(this);
    }

    @Override
    public void destroy() {
        mBrowserControlsStateProvider.removeObserver(this);
    }

    @Override
    public void onControlsOffsetChanged(int topOffset, int topControlsMinHeightOffset,
            int bottomOffset, int bottomControlsMinHeightOffset, boolean needsAnimate) {
        mDelegateObserver.onControlsOffsetChanged();
    }

    @Override
    public void onBottomControlsHeightChanged(
            int bottomControlsHeight, int bottomControlsMinHeight) {
        mDelegateObserver.onBottomControlsHeightChanged();
    }
}
