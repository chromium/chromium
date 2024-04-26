// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tab_activity_glue;

import androidx.annotation.NonNull;

import org.chromium.base.Log;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManager;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabReparentingParams;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/**
 * Controls the reparenting of tabs when an app restart is required due to configuration changes.
 * Tabs are preserved when the app is restarted under the following conditions:
 * - The current app theme changes.
 * - The layout switches between tablet/phone.
 * - (Keep this list up to date by adding future conditions here)
 */
public class TabReparentingController {
    /** Provides data to {@link TabReparentingController} facilitate reparenting tabs. */
    public interface Delegate {
        /** Gets a {@link TabModelSelector} which is used to add the tab. */
        TabModelSelector getTabModelSelector();

        /**
         * @return Whether the given Url is an NTP url, exists solely to support unit testing.
         */
        boolean isNtpUrl(GURL url);
    }

    private static final String TAG =
            "org.chromium.chrome.browser.app.tab_activity_glue.TabReparentingController";

    private final Delegate mDelegate;
    private final AsyncTabParamsManager mAsyncTabParamsManager;

    /** Constructs a {@link TabReparentingController} with the given delegate. */
    public TabReparentingController(
            @NonNull Delegate delegate, @NonNull AsyncTabParamsManager asyncTabParamsManager) {
        mDelegate = delegate;
        mAsyncTabParamsManager = asyncTabParamsManager;
    }

    /**
     * Prepares the tabs for reparenting by, 1. Informing the {@link TabModelSelector} that
     * reparenting is in progress. 2. Detaching each tab from the models. 3. For each tab that's
     * detached, it's added to {@link AsyncTabParamsManager}. These tabs are held in memory until an
     * application restart.
     *
     * <p>On app restart, the tabs from AsyncTabParamsManager are reattached/enabled in {@link
     * ChromeTabCreator}.
     */
    public void prepareTabsForReparenting() {
        // TODO(crbug.com/40124038): Make tab models detachable.
        TabModelSelector selector = mDelegate.getTabModelSelector();

        // Close tabs pending closure before saving params.
        selector.getModel(false).commitAllTabClosures();
        selector.getModel(true).commitAllTabClosures();

        // Aggregate all the tabs.
        List<Tab> tabs = new ArrayList<>(selector.getTotalTabCount());
        populateComprehensiveTabsFromModel(selector.getModel(false), tabs);
        populateComprehensiveTabsFromModel(selector.getModel(true), tabs);

        // Save all the tabs in memory to be retrieved after restart.
        mDelegate.getTabModelSelector().enterReparentingMode();
        int tabsAwaitingReparenting = 0;
        int tabsStillLoading = 0;
        for (int i = 0; i < tabs.size(); i++) {
            Tab tab = tabs.get(i);
            if (tab.isLoading()) {
                tab.stopLoading();
                tab.getWebContents().getNavigationController().setNeedsReload();
                tabsStillLoading++;
            }

            // The current tab has already been detached/stored and is waiting for android to
            // recreate the activity.
            if (mAsyncTabParamsManager.hasParamsForTabId(tab.getId())) {
                tabsAwaitingReparenting++;
                continue;
            }
            // Intentionally skip new tab pages and allow them to reload and restore scroll
            // state themselves.
            if (mDelegate.isNtpUrl(tab.getUrl())) continue;

            TabReparentingParams params = new TabReparentingParams(tab, null);
            mAsyncTabParamsManager.add(tab.getId(), params);
            ReparentingTask.from(tab).detach();

            tabsAwaitingReparenting++;
        }

        // TODO(crbug.com/40793204): Remove logging once root cause of bug is identified &
        //  fixed.
        Log.i(
                TAG,
                "#prepareTabsForReparenting, num tabs awaiting reparenting: "
                        + tabsAwaitingReparenting
                        + ", num tabs still loading: "
                        + tabsStillLoading);
    }

    protected static void populateComprehensiveTabsFromModel(TabModel model, List<Tab> outputTabs) {
        TabList tabList = model.getComprehensiveModel();
        for (int i = 0; i < tabList.getCount(); i++) {
            outputTabs.add(tabList.getTabAt(i));
        }
    }
}
