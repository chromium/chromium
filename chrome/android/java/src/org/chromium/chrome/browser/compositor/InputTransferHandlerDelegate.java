// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.GestureListenerManager;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.InputTransferHandler;
import org.chromium.content_public.browser.WebContents;

public class InputTransferHandlerDelegate extends ActivityTabTabObserver
        implements InputTransferHandler.Delegate {
    private Tab mCurrentTab;
    @VisibleForTesting public int mScrollOffsetY;
    private final GestureStateListener mGestureStateListener;

    public InputTransferHandlerDelegate(ActivityTabProvider provider) {
        super(provider);
        mGestureStateListener =
                new GestureStateListener() {
                    @Override
                    public void onScrollOffsetOrExtentChanged(
                            int scrollOffsetY, int scrollExtentY) {
                        mScrollOffsetY = scrollOffsetY;
                    }
                };
    }

    @Override
    protected void onObservingDifferentTab(Tab tab) {
        if (mCurrentTab != null) {
            WebContents webContents = mCurrentTab.getWebContents();
            if (webContents != null) {
                GestureListenerManager manager =
                        GestureListenerManager.fromWebContents(webContents);
                manager.removeListener(mGestureStateListener);
            }
        }

        if (tab != null) {
            WebContents webContents = tab.getWebContents();
            if (webContents != null) {
                GestureListenerManager manager =
                        GestureListenerManager.fromWebContents(webContents);
                manager.addListener(mGestureStateListener);
            }
        }

        mCurrentTab = tab;
    }

    @Override
    public boolean canTransferInputToViz() {
        // Do not transfer input when on top since overscroll controller might start a refresh
        // effect, which is not yet fixed to work with InputOnViz.
        return mScrollOffsetY != 0;
    }
}
