// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.directactions;

import android.os.Bundle;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.MenuOrKeyboardActionController;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * Exposes some well-known menu actions as direct actions.
 *
 * <p>This handler exposes a subset of available menu item actions, exposed by the given {@link
 * MenuOrKeyboardActionController}. By default, no actions are exposed; call {@link
 * #whitelistActions} or {@link #allowAllActions} to enable them.
 */
class MenuDirectActionHandler implements DirectActionHandler {
    /** Maps some menu item actions to direct actions. */
    private static final Map<String, Integer> ACTION_MAP;
    static {
        Map<String, Integer> map = new HashMap<>();
        map.put(ChromeDirectActionIds.GO_FORWARD, R.id.forward_menu_id);
        map.put(ChromeDirectActionIds.RELOAD, R.id.reload_menu_id);
        map.put(ChromeDirectActionIds.BOOKMARK_THIS_PAGE, R.id.bookmark_this_page_id);
        map.put(ChromeDirectActionIds.DOWNLOADS, R.id.downloads_menu_id);
        map.put(ChromeDirectActionIds.HELP, R.id.help_id);
        map.put(ChromeDirectActionIds.NEW_TAB, R.id.new_tab_menu_id);
        map.put(ChromeDirectActionIds.OPEN_HISTORY, R.id.open_history_menu_id);
        map.put(ChromeDirectActionIds.PREFERENCES, R.id.preferences_id);
        map.put(ChromeDirectActionIds.CLOSE_ALL_TABS, R.id.close_all_tabs_menu_id);
        ACTION_MAP = Collections.unmodifiableMap(map);
    }

    private final MenuOrKeyboardActionController mMenuOrKeyboardActionController;
    private final TabModelSelector mTabModelSelector;

    /** If non-null, only actions that belong to this whitelist are available. */
    @Nullable
    private Set<Integer> mActionIdWhitelist = new HashSet<>();

    MenuDirectActionHandler(MenuOrKeyboardActionController menuOrKeyboardActionController,
            TabModelSelector tabModelSelector) {
        this.mMenuOrKeyboardActionController = menuOrKeyboardActionController;
        this.mTabModelSelector = tabModelSelector;
    }

    /** Allows the use of all known actions. */
    void allowAllActions() {
        mActionIdWhitelist = null;
    }

    /**
     * Allows the use of the specified action, identified by their menu item id.
     *
     * <p>Does nothing if the actions are already available.
     */
    void whitelistActions(Integer... itemIds) {
        if (mActionIdWhitelist == null) return;

        for (int itemId : itemIds) {
            mActionIdWhitelist.add(itemId);
        }
    }

    @Override
    public void reportAvailableDirectActions(DirectActionReporter reporter) {
        Set<Integer> availableItemIds = new HashSet<>();
        Tab currentTab = mTabModelSelector.getCurrentTab();
        if (currentTab != null && currentTab.isUserInteractable()) {
            if (currentTab.canGoForward()) {
                availableItemIds.add(R.id.forward_menu_id);
            }
            availableItemIds.add(R.id.reload_menu_id);
            availableItemIds.add(R.id.bookmark_this_page_id);
            availableItemIds.add(R.id.open_history_menu_id);
        }
        if (mTabModelSelector.getTotalTabCount() > 0) {
            availableItemIds.add(R.id.close_all_tabs_menu_id);
        }

        availableItemIds.add(R.id.downloads_menu_id);
        availableItemIds.add(R.id.help_id);
        availableItemIds.add(R.id.new_tab_menu_id);
        availableItemIds.add(R.id.preferences_id);

        if (mActionIdWhitelist != null) availableItemIds.retainAll(mActionIdWhitelist);

        for (Map.Entry<String, Integer> entry : ACTION_MAP.entrySet()) {
            if (availableItemIds.contains(entry.getValue())) {
                reporter.addDirectAction(entry.getKey());
            }
        }
    }

    @Override
    public boolean performDirectAction(
            String actionId, Bundle arguments, Callback<Bundle> callback) {
        Integer menuId = ACTION_MAP.get(actionId);
        if (menuId != null && (mActionIdWhitelist == null || mActionIdWhitelist.contains(menuId))
                && mMenuOrKeyboardActionController.onMenuOrKeyboardAction(
                        menuId, /* fromMenu= */ false)) {
            callback.onResult(Bundle.EMPTY);
            return true;
        }
        return false;
    }
}
