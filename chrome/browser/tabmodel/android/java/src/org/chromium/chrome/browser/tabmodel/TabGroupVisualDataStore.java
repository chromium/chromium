// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.content.Context;
import android.content.SharedPreferences;
import android.text.TextUtils;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.tab_groups.TabGroupColorId;

/**
 * Helper class to handle persistence of tab group metadata. This includes the title, color, and
 * collapsed state.
 */
@NullMarked
class TabGroupVisualDataStore {
    private static final String TAB_GROUP_TITLES_FILE_NAME = "tab_group_titles";
    private static final String TAB_GROUP_COLORS_FILE_NAME = "tab_group_colors";
    private static final String TAB_GROUP_COLLAPSED_FILE_NAME = "tab_group_collapsed";

    // Title methods

    /**
     * This method stores tab group title with reference to {@code tabRootId}. Package protected as
     * all access should route through the {@link TabGroupModelFilter}.
     *
     * @param tabRootId The tab root ID which is used as reference to store group title.
     * @param title The tab group title to store.
     */
    static void storeTabGroupTitle(int tabRootId, @Nullable String title) {
        assert tabRootId != Tab.INVALID_TAB_ID;
        if (TextUtils.isEmpty(title)) {
            deleteTabGroupTitle(tabRootId);
        } else {
            getTitleSharedPreferences().edit().putString(String.valueOf(tabRootId), title).apply();
        }
    }

    /**
     * This method deletes specific stored tab group title with reference to {@code tabRootId}.
     * While currently public, the intent is to make this package protected and force all access to
     * go through the {@Link TabGroupModelFilter}.
     *
     * @param tabRootId The tab root ID whose related tab group title will be deleted.
     */
    static void deleteTabGroupTitle(int tabRootId) {
        assert tabRootId != Tab.INVALID_TAB_ID;
        getTitleSharedPreferences().edit().remove(String.valueOf(tabRootId)).apply();
    }

    /**
     * This method fetches tab group title with related tab group root ID. While currently public,
     * the intent is to make this package protected and force all access to go through the {@Link
     * TabGroupModelFilter}.
     *
     * @param tabRootId The tab root ID whose related tab group title will be fetched.
     * @return The stored title of the target tab group, default value is null.
     */
    static @Nullable String getTabGroupTitle(int tabRootId) {
        assert tabRootId != Tab.INVALID_TAB_ID;
        // TODO(crbug.com/40895368): Consider checking if this looks like the default plural string
        // and deleting and returning null if any users have saved tab group titles.
        return getTitleSharedPreferences().getString(String.valueOf(tabRootId), null);
    }

    private static SharedPreferences getTitleSharedPreferences() {
        return ContextUtils.getApplicationContext()
                .getSharedPreferences(TAB_GROUP_TITLES_FILE_NAME, Context.MODE_PRIVATE);
    }

    // Color methods

    /**
     * This method stores tab group colors with reference to {@code tabRootId}. Package protected as
     * all access should route through the {@link TabGroupModelFilter}.
     *
     * @param tabRootId The tab root ID which is used as a reference to store group colors.
     * @param color The tab group color {@link TabGroupColorId} to store.
     */
    static void storeTabGroupColor(int tabRootId, int color) {
        assert tabRootId != Tab.INVALID_TAB_ID;
        getColorSharedPreferences().edit().putInt(String.valueOf(tabRootId), color).apply();
    }

    /**
     * This method deletes a specific stored tab group color with reference to {@code tabRootId}.
     * While currently public, the intent is to make this package protected and force all access to
     * go through the {@Link TabGroupModelFilter}.
     *
     * @param tabRootId The tab root ID whose related tab group color will be deleted.
     */
    static void deleteTabGroupColor(int tabRootId) {
        assert tabRootId != Tab.INVALID_TAB_ID;
        getColorSharedPreferences().edit().remove(String.valueOf(tabRootId)).apply();
    }

    /**
     * This method fetches tab group colors for the related tab group root ID. While currently
     * public, the intent is to make thisUndo package protected and force all access to go through
     * the {@Link TabGroupModelFilter}.
     *
     * @param tabRootId The tab root ID whose related tab group color will be fetched.
     * @return The stored color of the target tab group, default value is -1 (INVALID_COLOR_ID).
     */
    static int getTabGroupColor(int tabRootId) {
        assert tabRootId != Tab.INVALID_TAB_ID;
        return getColorSharedPreferences()
                .getInt(String.valueOf(tabRootId), TabGroupColorUtils.INVALID_COLOR_ID);
    }

    private static SharedPreferences getColorSharedPreferences() {
        return ContextUtils.getApplicationContext()
                .getSharedPreferences(TAB_GROUP_COLORS_FILE_NAME, Context.MODE_PRIVATE);
    }

    // Collapsed methods

    /**
     * @param tabRootId The tab root ID whose related group's collapsed state will be stored.
     * @param isCollapsed If the tab group is collapsed or expanded.
     */
    static void storeTabGroupCollapsed(int tabRootId, boolean isCollapsed) {
        assert tabRootId != Tab.INVALID_TAB_ID;
        if (isCollapsed) {
            getCollapsedSharedPreferences()
                    .edit()
                    .putBoolean(String.valueOf(tabRootId), true)
                    .apply();
        } else {
            deleteTabGroupCollapsed(tabRootId);
        }
    }

    /**
     * @param tabRootId The tab root ID whose related tab group collapsed state will be deleted.
     */
    static void deleteTabGroupCollapsed(int tabRootId) {
        assert tabRootId != Tab.INVALID_TAB_ID;
        getCollapsedSharedPreferences().edit().remove(String.valueOf(tabRootId)).apply();
    }

    /**
     * @param tabRootId The tab root ID whose related group's collapsed state will be fetched.
     * @return Whether the tab group is collapsed or expanded.
     */
    static boolean getTabGroupCollapsed(int tabRootId) {
        assert tabRootId != Tab.INVALID_TAB_ID;
        return getCollapsedSharedPreferences().getBoolean(String.valueOf(tabRootId), false);
    }

    private static SharedPreferences getCollapsedSharedPreferences() {
        return ContextUtils.getApplicationContext()
                .getSharedPreferences(TAB_GROUP_COLLAPSED_FILE_NAME, Context.MODE_PRIVATE);
    }
}
