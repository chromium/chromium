// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.os.Handler;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabAssociatedApp;

/**
 * Implementation of {@link NavigationHandler#ActionDelegate} that works with
 * native/rendered pages in tabbed mode. Uses interface methods of {@link Tab}
 * and {@link ChromeActivity} to implement navigation.
 */
public class TabbedActionDelegate implements NavigationHandler.ActionDelegate {
    private final Tab mTab;
    private final Handler mHandler = new Handler();

    public TabbedActionDelegate(Tab tab) {
        mTab = tab;
    }

    @Override
    public boolean canNavigate(boolean forward) {
        return forward ? mTab.canGoForward() : true;
    }

    @Override
    public void navigate(boolean forward) {
        if (forward) {
            mTab.goForward();
        } else {
            // Perform back action at the next UI thread execution. The back action can
            // potentially close the tab we're running on, which causes use-after-destroy
            // exception if the closing operation is performed synchronously.
            mHandler.post(() -> mTab.getActivity().onBackPressed());
        }
    }

    @Override
    public boolean willBackCloseTab() {
        return !mTab.canGoBack() && mTab.getActivity().backShouldCloseTab(mTab);
    }

    @Override
    public boolean willBackExitApp() {
        return !mTab.canGoBack()
                && (!mTab.getActivity().backShouldCloseTab(mTab)
                        || TabAssociatedApp.isOpenedFromExternalApp(mTab));
    }
}
