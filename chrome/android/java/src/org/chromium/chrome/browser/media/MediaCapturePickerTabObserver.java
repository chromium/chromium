// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.app.tabmodel.AllTabObserver;
import org.chromium.chrome.browser.tab.Tab;

/** An observer that filters tabs and forwards events to a delegate. */
@NullMarked
public class MediaCapturePickerTabObserver implements AllTabObserver.Observer {
    private final AllTabObserver.Observer mDelegate;
    private final MediaCapturePickerManager.Params mParams;

    public MediaCapturePickerTabObserver(
            AllTabObserver.Observer delegate, MediaCapturePickerManager.Params params) {
        mDelegate = delegate;
        mParams = params;
    }

    @Override
    public void onTabAdded(Tab tab) {
        // We do not support capture of native pages.
        if (tab.isNativePage()) return;

        // Filter out all tabs that are not this tab for capture this tab.
        if (mParams.captureThisTab && tab.getWebContents() != mParams.webContents) return;

        mDelegate.onTabAdded(tab);
    }

    @Override
    public void onTabRemoved(Tab tab) {
        // We do not support capture of native pages.
        if (tab.isNativePage()) return;

        // Filter out all tabs that are not this tab for capture this tab.
        if (mParams.captureThisTab && tab.getWebContents() != mParams.webContents) return;

        mDelegate.onTabRemoved(tab);
    }
}
