// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.net.NetError;
import org.chromium.url.GURL;

/**
 * Monitor changes that indicate a theme color change may be needed from tab contents.
 */
public class TabThemeColorHelper extends EmptyTabObserver {
    private final Tab mTab;
    private final Runnable mUpdateRunnable;

    TabThemeColorHelper(Tab tab, Runnable updateRunnable) {
        mTab = tab;
        mUpdateRunnable = updateRunnable;
        tab.addObserver(this);
    }

    /**
     * Notifies the listeners of the tab theme color change.
     */
    void updateIfNeeded() {
        mUpdateRunnable.run();
    }

    // TabObserver

    @Override
    public void onSSLStateUpdated(Tab tab) {
        updateIfNeeded();
    }

    @Override
    public void onUrlUpdated(Tab tab) {
        updateIfNeeded();
    }

    @Override
    public void onDidFailLoad(Tab tab, boolean isMainFrame, int errorCode, GURL failingUrl) {
        updateIfNeeded();
    }

    @Override
    public void onDidFinishNavigation(Tab tab, NavigationHandle navigation) {
        if (navigation.errorCode() != NetError.OK) updateIfNeeded();
    }

    @Override
    public void onDestroyed(Tab tab) {
        tab.removeObserver(this);
    }
}
