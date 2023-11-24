// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;

/** Handles the visibility update of the activity tab. */
public class AccessibilityVisibilityHandler
        implements DestroyObserver, TabObscuringHandler.Observer {
    private final ActivityTabProvider.ActivityTabTabObserver mActivityTabObserver;
    private final TabObscuringHandler mTabObscuringHandler;
    private TabImpl mTab;
    private boolean mIsWebContentObscured;

    public AccessibilityVisibilityHandler(
            ActivityLifecycleDispatcher lifecycleDispatcher,
            ActivityTabProvider activityTabProvider,
            TabObscuringHandler tabObscuringHandler) {
        mActivityTabObserver =
                new ActivityTabProvider.ActivityTabTabObserver(activityTabProvider) {
                    @Override
                    public void onObservingDifferentTab(Tab tab, boolean hint) {
                        if (mTab == tab) return;
                        if (mTab != null) {
                            updateObscured(false, false);
                        }
                        mTab = (TabImpl) tab;
                        if (mTab != null) {
                            updateObscured(
                                    mTabObscuringHandler.isTabContentObscured(),
                                    mTabObscuringHandler.isToolbarObscured());
                        }
                    }

                    @Override
                    public void onContentChanged(Tab tab) {
                        updateObscured(
                                mTabObscuringHandler.isTabContentObscured(),
                                mTabObscuringHandler.isToolbarObscured());
                    }
                };
        mTabObscuringHandler = tabObscuringHandler;
        mTabObscuringHandler.addObserver(this);
        lifecycleDispatcher.register(this);
    }

    // TabObscuringHandler.Observer

    @Override
    public void updateObscured(boolean obscureTabContent, boolean obscureToolbar) {
        if (mTab == null) return;
        boolean isWebContentObscured = obscureTabContent || mTab.isShowingCustomView();
        mTab.updateWebContentObscured(isWebContentObscured);
    }

    // DestroyObserver

    @Override
    public void onDestroy() {
        mActivityTabObserver.destroy();
        mTabObscuringHandler.removeObserver(this);
        mTab = null;
    }
}
