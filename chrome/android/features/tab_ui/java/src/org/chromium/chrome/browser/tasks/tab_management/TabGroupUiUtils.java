// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import androidx.annotation.StringRes;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.tabmodel.TabGroupUtils;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.tab_ui.R;

import java.util.Collection;
import java.util.Collections;

/** Helper methods for Tab Group UI components and string resources. */
@NullMarked
public class TabGroupUiUtils {

    /**
     * Returns the string resource ID for the 'add to group' menu item ("Add tab to group" vs "Add
     * tab to new group" vs "Move tab to group").
     *
     * @param currentTabGroupId The tab group ID of the current tab if already in a group, or null.
     * @param hasTabGroups Whether any tab groups exist.
     */
    public static @StringRes int getAddToGroupMenuItemString(
            @Nullable Token currentTabGroupId, boolean hasTabGroups) {
        if (currentTabGroupId != null) {
            return R.string.menu_move_tab_to_group;
        }
        return hasTabGroups ? R.string.menu_add_tab_to_group : R.string.menu_add_tab_to_new_group;
    }

    /**
     * Returns the string resource ID for the 'add to group' menu item ("Add tab to group" vs "Add
     * tab to new group" vs "Move tab to group").
     *
     * @param tabModel The current {@link TabModel}.
     * @param currentTabGroupId The tab group ID of the current tab if already in a group, or null.
     * @param checkAllWindows Whether to check across all active windows for existing tab groups.
     */
    public static @StringRes int getAddToGroupMenuItemString(
            @Nullable TabModel tabModel,
            @Nullable Token currentTabGroupId,
            boolean checkAllWindows) {
        if (currentTabGroupId != null) {
            return R.string.menu_move_tab_to_group;
        }
        Collection<TabModelSelector> selectors =
                checkAllWindows
                        ? TabWindowManagerSingleton.getInstance().getAllTabModelSelectors()
                        : Collections.emptyList();
        return getAddToGroupMenuItemString(
                currentTabGroupId, TabGroupUtils.hasTabGroups(tabModel, selectors));
    }

    /**
     * Returns the string resource ID for the 'add to group' menu item.
     *
     * @param tabModel The current {@link TabModel}.
     * @param currentTabGroupId The tab group ID of the current tab if already in a group, or null.
     */
    public static @StringRes int getAddToGroupMenuItemString(
            @Nullable TabModel tabModel, @Nullable Token currentTabGroupId) {
        return getAddToGroupMenuItemString(
                tabModel, currentTabGroupId, /* checkAllWindows= */ false);
    }
}
