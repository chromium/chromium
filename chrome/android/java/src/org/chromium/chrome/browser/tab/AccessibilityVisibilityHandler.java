// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;

/**
 * Handles the visibility update of the activity tab.
 */
public class AccessibilityVisibilityHandler implements DestroyObserver {
    private final ActivityTabProvider.ActivityTabTabObserver mActivityTabObserver;
    private TabImpl mTab;

    public AccessibilityVisibilityHandler(ActivityLifecycleDispatcher lifecycleDispatcher,
            ActivityTabProvider activityTabProvider, TabObscuringHandler tabObscuringHandler) {
        mActivityTabObserver = new ActivityTabProvider.ActivityTabTabObserver(activityTabProvider) {
            @Override
            public void onObservingDifferentTab(Tab tab, boolean hint) {
                if (mTab == tab) return;

                TabImpl tabImpl = (TabImpl) tab;
                if (mTab != null) tabObscuringHandler.removeObserver(mTab);
                if (tabImpl != null) tabObscuringHandler.addObserver(tabImpl);
                mTab = tabImpl;
            }

            @Override
            public void onContentChanged(Tab tab) {
                mTab.updateObscured(tabObscuringHandler.isTabContentObscured(),
                        tabObscuringHandler.isToolbarObscured());
            }
        };
        lifecycleDispatcher.register(this);
    }

    // DestroyObserver

    @Override
    public void onDestroy() {
        mActivityTabObserver.destroy();
        mTab = null;
    }
}
