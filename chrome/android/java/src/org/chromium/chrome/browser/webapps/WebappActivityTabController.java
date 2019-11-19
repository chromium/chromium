// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;
import android.os.Bundle;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.HintlessActivityTabObserver;
import org.chromium.chrome.browser.browserservices.BrowserServicesActivityTabController;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.customtabs.content.TabCreationMode;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

import javax.inject.Inject;

/**
 * Shortcut/WebAPK implementation of {@link BrowserServicesActivityTabController}.
 */
public class WebappActivityTabController implements BrowserServicesActivityTabController {
    private final ActivityTabProvider mActivityTabProvider;
    private final CustomTabActivityTabProvider mTabProvider;
    private final TabObserverRegistrar mTabObserverRegistrar;

    private HintlessActivityTabObserver mTabSwapObserver = new HintlessActivityTabObserver() {
        @Override
        public void onActivityTabChanged(@Nullable Tab tab) {
            mTabProvider.swapTab(tab);
        }
    };

    @Inject
    public WebappActivityTabController(ActivityTabProvider activityTabProvider,
            CustomTabActivityTabProvider tabProvider, TabObserverRegistrar tabObserverRegistrar) {
        mActivityTabProvider = activityTabProvider;
        mTabProvider = tabProvider;
        mTabObserverRegistrar = tabObserverRegistrar;
    }

    public void setInitialTab(Tab tab) {
        mTabProvider.setInitialTab(tab, TabCreationMode.DEFAULT);
        mActivityTabProvider.addObserverAndTrigger(mTabSwapObserver);
        mTabObserverRegistrar.addObserversForTab(tab);
    }

    @Override
    public void detachAndStartReparenting(
            Intent intent, Bundle startActivityOptions, Runnable finishCallback) {}

    @Override
    public void closeTab() {
        Tab tab = mTabProvider.getTab();
        if (tab == null) return;

        tab.getActivity().getTabModelSelector().closeAllTabs(true);
    }

    @Override
    public void closeAndForgetTab() {
        closeTab();
    }

    @Override
    public void saveState() {}

    @Override
    @Nullable
    public TabModelSelector getTabModelSelector() {
        Tab tab = mTabProvider.getTab();
        if (tab == null) return null;

        return tab.getActivity().getTabModelSelector();
    }
}
