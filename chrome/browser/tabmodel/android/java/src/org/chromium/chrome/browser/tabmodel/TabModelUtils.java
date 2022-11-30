// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.content_public.browser.WebContents;

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
     * @return      {@code true} if the {@link Tab} was found.
     */
    public static boolean closeTabByIndex(TabModel model, int index) {
        Tab tab = model.getTabAt(index);
        if (tab == null) return false;

        return model.closeTab(tab);
    }

    /**
     * @param model The {@link TabModel} to act on.
     * @param tabId The id of the {@link Tab} to close.
     * @return      {@code true} if the {@link Tab} was found.
     */
    public static boolean closeTabById(TabModel model, int tabId) {
        return closeTabById(model, tabId, false);
    }

    /**
     * @param model   The {@link TabModel} to act on.
     * @param tabId   The id of the {@link Tab} to close.
     * @param canUndo Whether or not this closure can be undone.
     * @return        {@code true} if the {@link Tab} was found.
     */
    public static boolean closeTabById(TabModel model, int tabId, boolean canUndo) {
        Tab tab = TabModelUtils.getTabById(model, tabId);
        if (tab == null) return false;

        return model.closeTab(tab, true, false, canUndo);
    }

    /**
     * @param model The {@link TabModel} to act on.
     * @return      {@code true} if the {@link Tab} was found.
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
            if (tab.getId() == tabId) return i;
        }

        return TabModel.INVALID_TAB_INDEX;
    }

    /**
     * Find the {@link Tab} with the specified id.
     * @param model The {@link TabModel} to act on.
     * @param tabId The id of the {@link Tab} to find.
     * @return      Specified {@link Tab} or {@code null} if the {@link Tab} is not found
     */
    public static Tab getTabById(TabList model, int tabId) {
        int index = getTabIndexById(model, tabId);
        if (index == TabModel.INVALID_TAB_INDEX) return null;
        return model.getTabAt(index);
    }

    /**
     * Find the {@link Tab} index whose URL matches the specified URL.
     * @param model The {@link TabModel} to act on.
     * @param url   The URL to search for.
     * @return      Specified {@link Tab} or {@code null} if the {@link Tab} is not found
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
     * A helper method that automatically passes {@link TabSelectionType#FROM_USER} as the selection
     * type to {@link TabModel#setIndex(int, TabSelectionType)}.
     * @param model The {@link TabModel} to act on.
     * @param index The index of the {@link Tab} to select.
     * @param skipLoadingTab Whether to skip loading the Tab.
     */
    public static void setIndex(TabModel model, int index, boolean skipLoadingTab) {
        setIndex(model, index, skipLoadingTab, TabSelectionType.FROM_USER);
    }

    /**
     * A helper method that allows specifying a {@link TabSelectionType}
     * type to {@link TabModel#setIndex(int, TabSelectionType)}.
     * @param model The {@link TabModel} to act on.
     * @param index The index of the {@link Tab} to select.
     * @param skipLoadingTab Whether to skip loading the Tab.
     * @param type {@link TabSelectionType} how the tab selection was initiated.
     */
    public static void setIndex(
            TabModel model, int index, boolean skipLoadingTab, @TabSelectionType int type) {
        model.setIndex(index, type, skipLoadingTab);
    }

    /**
     * Returns all the Tabs in the specified TabList that were opened from the Tab with the
     * specified ID. The returned Tabs are in the same order as in the TabList.
     * @param model The {@link TabModel} to act on.
     * @param tabId The ID of the Tab whose children should be returned.
     */
    public static List<Tab> getChildTabs(TabList model, int tabId) {
        Tab tab = model.getTabAt(tabId);

        ArrayList<Tab> childTabs = new ArrayList<Tab>();
        for (int i = 0; i < model.getCount(); i++) {
            if (CriticalPersistedTabData.from(model.getTabAt(i)).getParentId() == tabId) {
                childTabs.add(model.getTabAt(i));
            }
        }

        return childTabs;
    }

    /**
     * @return all regular {@link Tab} ids from a {@link TabModelSelectoor}
     */
    public static List<Integer> getRegularTabIds(TabModelSelector tabModelSelector) {
        List<Integer> tabIds = new ArrayList<>();
        int numTabs = tabModelSelector.getModel(false).getCount();
        for (int i = 0; i < numTabs; i++) {
            tabIds.add(tabModelSelector.getModel(false).getTabAt(i).getId());
        }
        return tabIds;
    }

    /**
     * Returns the most recently visited Tab in the specified TabList that is not {@code tabId}.
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

            final long currentTime = CriticalPersistedTabData.from(currentTab).getTimestampMillis();
            if (currentTime != CriticalPersistedTabData.INVALID_TIMESTAMP
                    && mostRecentTabTime < currentTime) {
                mostRecentTabTime = currentTime;
                mostRecentTab = currentTab;
            }
        }
        return mostRecentTab;
    }
}
