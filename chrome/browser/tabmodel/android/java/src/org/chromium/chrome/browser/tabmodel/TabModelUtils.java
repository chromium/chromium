// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

import java.util.ArrayList;
import java.util.List;

/**
 * A set of convenience methods used for interacting with {@link TabList}s and {@link TabModel}s.
 */
public class TabModelUtils {
    private TabModelUtils() {}

    /**
     * @param model The {@link TabModel} to act on.
     * @param index The index of the {@link Tab} to close.
     * @return {@code true} if the {@link Tab} was found.
     */
    public static boolean closeTabByIndex(TabModel model, int index) {
        Tab tab = model.getTabAt(index);
        if (tab == null) return false;

        return model.closeTab(tab);
    }

    /**
     * @param model The {@link TabModel} to act on.
     * @param tabId The id of the {@link Tab} to close.
     * @param canUndo Whether or not this closure can be undone.
     * @return {@code true} if the {@link Tab} was found.
     */
    public static boolean closeTabById(TabModel model, int tabId, boolean canUndo) {
        Tab tab = model.getTabById(tabId);
        if (tab == null || tab.isClosing()) return false;

        return model.closeTab(tab, false, canUndo);
    }

    /**
     * @param model The {@link TabModel} to act on.
     * @return {@code true} if the {@link Tab} was found.
     */
    public static boolean closeCurrentTab(TabModel model) {
        Tab tab = TabModelUtils.getCurrentTab(model);
        if (tab == null) return false;

        return model.closeTab(tab);
    }

    /**
     * Find the index of the {@link Tab} with the specified id.
     * @param model The {@link TabModel} to act on.
     * @param tabId The id of the {@link Tab} to find.
     * @return      Specified {@link Tab} index or {@link TabList#INVALID_TAB_INDEX} if the
     *              {@link Tab} is not found
     */
    public static int getTabIndexById(TabList model, int tabId) {
        int count = model.getCount();

        for (int i = 0; i < count; i++) {
            Tab tab = model.getTabAt(i);
            assert tab != null : "getTabAt() shouldn't return a null Tab from TabModel.";
            if (tab != null && tab.getId() == tabId) return i;
        }

        return TabModel.INVALID_TAB_INDEX;
    }

    /**
     * Find the {@link Tab} index whose URL matches the specified URL.
     *
     * @param model The {@link TabModel} to act on.
     * @param url The URL to search for.
     * @return Specified {@link Tab} or {@code null} if the {@link Tab} is not found
     */
    public static int getTabIndexByUrl(TabList model, String url) {
        int count = model.getCount();

        for (int i = 0; i < count; i++) {
            if (model.getTabAt(i).getUrl().getSpec().contentEquals(url)) return i;
        }

        return TabModel.INVALID_TAB_INDEX;
    }

    /**
     * Get the currently selected {@link Tab} id.
     * @param model The {@link TabModel} to act on.
     * @return      The id of the currently selected {@link Tab}.
     */
    public static int getCurrentTabId(TabList model) {
        Tab tab = getCurrentTab(model);
        if (tab == null) return Tab.INVALID_TAB_ID;

        return tab.getId();
    }

    /**
     * Get the currently selected {@link Tab}.
     * @param model The {@link TabModel} to act on.
     * @returns     The current {@link Tab} or {@code null} if no {@link Tab} is selected
     */
    public static Tab getCurrentTab(TabList model) {
        int index = model.index();
        if (index == TabModel.INVALID_TAB_INDEX) return null;

        return model.getTabAt(index);
    }

    /**
     * @param model The {@link TabModel} to act on.
     * @return      The currently active {@link WebContents}, or {@code null} if no {@link Tab}
     *              is selected or the selected {@link Tab} has no current {@link WebContents}.
     */
    public static WebContents getCurrentWebContents(TabList model) {
        Tab tab = getCurrentTab(model);
        if (tab == null) return null;

        return tab.getWebContents();
    }

    /**
     * Selects a tab by its ID in the tab model selector.
     *
     * @param selector The {@link TabModelSelector} to act on.
     * @param tabId The tab ID to select.
     * @param type {@link TabSelectionType} how the tab selection was initiated.
     */
    public static void selectTabById(
            @NonNull TabModelSelector selector, int tabId, @TabSelectionType int tabSelectionType) {
        if (tabId == Tab.INVALID_TAB_ID) return;

        TabModel model = selector.getModelForTabId(tabId);
        if (model == null) return;

        model.setIndex(getTabIndexById(model, tabId), tabSelectionType);
    }

    /**
     * A helper method that automatically passes {@link TabSelectionType#FROM_USER} as the selection
     * type to {@link TabModel#setIndex(int, TabSelectionType)}.
     *
     * @param model The {@link TabModel} to act on.
     * @param index The index of the {@link Tab} to select.
     */
    public static void setIndex(TabModel model, int index) {
        setIndex(model, index, TabSelectionType.FROM_USER);
    }

    /**
     * A helper method that allows specifying a {@link TabSelectionType} type to {@link
     * TabModel#setIndex(int, TabSelectionType)}.
     *
     * @param model The {@link TabModel} to act on.
     * @param index The index of the {@link Tab} to select.
     * @param type {@link TabSelectionType} how the tab selection was initiated.
     */
    public static void setIndex(TabModel model, int index, @TabSelectionType int type) {
        model.setIndex(index, type);
    }

    /**
     * Returns the most recently visited Tab in the specified TabList that is not {@code tabId}.
     *
     * @param model The {@link TabModel} to act on.
     * @param tabId The ID of the {@link Tab} to skip or {@link Tab.INVALID_TAB_ID}.
     * @return the most recently visited Tab or null if none can be found.
     */
    public static Tab getMostRecentTab(TabList model, int tabId) {
        Tab mostRecentTab = null;
        long mostRecentTabTime = 0;
        for (int i = 0; i < model.getCount(); i++) {
            final Tab currentTab = model.getTabAt(i);
            if (currentTab.getId() == tabId || currentTab.isClosing()) continue;

            final long currentTime = currentTab.getTimestampMillis();
            // TODO(b/301642179) Consider using Optional on Tab interface for getTimestampMillis()
            // to signal that the timestamp is unknown.
            if (currentTime != Tab.INVALID_TIMESTAMP && mostRecentTabTime < currentTime) {
                mostRecentTabTime = currentTime;
                mostRecentTab = currentTab;
            }
        }
        return mostRecentTab;
    }

    /**
     * Executes an {@link Callback} when {@link TabModelSelector#isTabStateInitialized()} becomes
     * true. This will happen immediately and synchronously if the tab state is already initialized.
     *
     * @param tabModelSelector The {@link TabModelSelector} to act on.
     * @param callback The callback to be run once tab state is initialized, receiving a
     *     tabModelSelector.
     */
    public static void runOnTabStateInitialized(
            @NonNull TabModelSelector tabModelSelector,
            @NonNull Callback<TabModelSelector> callback) {
        if (tabModelSelector.isTabStateInitialized()) {
            callback.onResult(tabModelSelector);
        } else {
            TabModelSelectorObserver observer =
                    new TabModelSelectorObserver() {
                        @Override
                        public void onTabStateInitialized() {
                            tabModelSelector.removeObserver(this);
                            callback.onResult(tabModelSelector);
                        }
                    };

            tabModelSelector.addObserver(observer);
        }
    }

    /**
     * @param tab The {@link Tab} to find the {@link TabModelFilter} for.
     * @return the associated {@link TabModelFilter} if found.
     */
    public static TabModelFilter getTabModelFilterByTab(@NonNull Tab tab) {
        final WindowAndroid windowAndroid = tab.getWindowAndroid();
        if (windowAndroid == null) return null;

        final ObservableSupplier<TabModelSelector> supplier =
                TabModelSelectorSupplier.from(windowAndroid);
        if (supplier == null) return null;

        final TabModelSelector selector = supplier.get();
        if (selector == null) return null;

        return selector.getTabModelFilterProvider().getTabModelFilter(tab.isIncognito());
    }

    /** Converts a {@link TabList} to a {@link List<Tab>}. A null input returns an empty list. */
    public static @Nullable List<Tab> convertTabListToListOfTabs(@Nullable TabList tabList) {
        ArrayList<Tab> list = new ArrayList<>();
        if (tabList == null) return list;

        for (int i = 0; i < tabList.getCount(); i++) {
            list.add(tabList.getTabAt(i));
        }
        return list;
    }
}
