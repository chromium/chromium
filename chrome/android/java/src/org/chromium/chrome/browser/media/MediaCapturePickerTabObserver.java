// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.app.tabmodel.AllTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;

/** An observer that filters tabs and forwards events to a delegate. */
@NullMarked
public class MediaCapturePickerTabObserver implements AllTabObserver.Observer {
    /** A delegate to handle filtering of tabs. */
    public interface FilterDelegate {
        /**
         * Called to check if a tab should be filtered.
         *
         * @param webContents The contents to check.
         * @return True if the tab should be filtered.
         */
        boolean shouldFilterWebContents(WebContents webContents);
    }

    private final AllTabObserver.Observer mObserverDelegate;
    private final MediaCapturePickerManager.Params mParams;
    private final FilterDelegate mFilterDelegate;

    public MediaCapturePickerTabObserver(
            AllTabObserver.Observer delegate,
            MediaCapturePickerManager.Params params,
            FilterDelegate filterDelegate) {
        mObserverDelegate = delegate;
        mParams = params;
        mFilterDelegate = filterDelegate;
    }

    @Override
    public void onTabAdded(Tab tab) {
        // We do not support capture of native pages.
        if (tab.isNativePage()) return;

        // Filter out all tabs that are not this tab for capture this tab.
        if (mParams.captureThisTab && tab.getWebContents() != mParams.webContents) return;

        if (shouldFilterTabForPolicy(tab)) return;

        mObserverDelegate.onTabAdded(tab);
    }

    @Override
    public void onTabRemoved(Tab tab) {
        // We do not support capture of native pages.
        if (tab.isNativePage()) return;

        // Filter out all tabs that are not this tab for capture this tab.
        if (mParams.captureThisTab && tab.getWebContents() != mParams.webContents) return;

        if (shouldFilterTabForPolicy(tab)) return;

        mObserverDelegate.onTabRemoved(tab);
    }

    private boolean shouldFilterTabForPolicy(Tab tab) {
        // TODO(crbug.com/352187279): Track when web contents is navigated and add/remove.
        final WebContents webContents = tab.getWebContents();
        if (webContents == null) return true;

        return mFilterDelegate.shouldFilterWebContents(webContents);
    }
}
