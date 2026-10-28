// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tab_activity_glue;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManager;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabReparentingParams;
import org.chromium.content_public.browser.WebContents;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;
import java.util.function.Supplier;

/**
 * Controls the reparenting of tabs when an app restart is required due to configuration changes.
 * Tabs are preserved when the app is restarted under the following conditions: - The current app
 * theme changes. - The layout switches between tablet/phone. - (Keep this list up to date by adding
 * future conditions here)
 */
@NullMarked
public class TabReparentingController {
    private static final String TAG =
            "org.chromium.chrome.browser.app.tab_activity_glue.TabReparentingController";

    private final Supplier<TabModelSelector> mTabModelSelectorSupplier;
    private final AsyncTabParamsManager mAsyncTabParamsManager;
    private final Runnable mDestroyTabPersistentStoreRunnable;

    /** Constructs a {@link TabReparentingController} with the given dependencies. */
    public TabReparentingController(
            Supplier<TabModelSelector> tabModelSelectorSupplier,
            AsyncTabParamsManager asyncTabParamsManager,
            Runnable destroyTabPersistentStoreRunnable) {
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mAsyncTabParamsManager = asyncTabParamsManager;
        mDestroyTabPersistentStoreRunnable = destroyTabPersistentStoreRunnable;
    }

    /**
     * Prepares the tabs for reparenting by:
     *
     * <ol>
     *   <li>Informing the {@link TabModelSelector} that reparenting is in progress.
     *   <li>Detaching each tab from the window.
     *   <li>Adding each detached tab to {@link AsyncTabParamsManager}. These tabs are held in
     *       memory until an application restart.
     * </ol>
     *
     * <p>On app restart, the tabs from {@link AsyncTabParamsManager} are detached from the old
     * {@link TabModelSelector} via {@link TabReparentingParams}'s finalize callback before
     * reattachment in {@link ChromeTabCreator}.
     */
    public void prepareTabsForReparenting() {
        TabModelSelector selector = mTabModelSelectorSupplier.get();
        if (selector == null) return;

        // Close tabs pending closure before saving params.
        selector.getModel(false).commitAllTabClosures();
        selector.getModel(true).commitAllTabClosures();

        // Aggregate all the tabs.
        List<Tab> tabs = new ArrayList<>(selector.getTotalTabCount());
        populateComprehensiveTabsFromModel(selector.getModel(false), tabs);
        populateComprehensiveTabsFromModel(selector.getModel(true), tabs);

        // Save all the tabs in memory to be retrieved after restart.
        selector.enterReparentingMode();
        WeakReference<TabModelSelector> weakSelector = new WeakReference<>(selector);
        WeakReference<Runnable> weakDestroyStore =
                new WeakReference<>(mDestroyTabPersistentStoreRunnable);
        int tabsAwaitingReparenting = 0;
        int tabsStillLoading = 0;
        for (int i = 0; i < tabs.size(); i++) {
            Tab tab = tabs.get(i);
            if (tab.isLoading()) {
                tab.stopLoading();
                WebContents webContents = tab.getWebContents();
                if (webContents != null) {
                    webContents.getNavigationController().setNeedsReload();
                }
                tabsStillLoading++;
            }

            // The current tab has already been detached/stored and is waiting for android to
            // recreate the activity.
            if (mAsyncTabParamsManager.hasParamsForTabId(tab.getId())) {
                tabsAwaitingReparenting++;
                continue;
            }

            TabReparentingParams params =
                    new TabReparentingParams(
                            tab,
                            () -> {
                                // Destroy old store first so its observer won't delete persisted
                                // tab/group rows the new Activity restores; weak refs keep
                                // AsyncTabParamsManager from retaining the old Activity.
                                Runnable destroyStore = weakDestroyStore.get();
                                if (destroyStore != null) {
                                    destroyStore.run();
                                }
                                TabModelSelector oldSelector = weakSelector.get();
                                if (oldSelector != null) {
                                    oldSelector.detachTabForReparenting(tab);
                                }
                            });
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
        for (Tab tab : tabList) {
            outputTabs.add(tab);
        }
    }
}
