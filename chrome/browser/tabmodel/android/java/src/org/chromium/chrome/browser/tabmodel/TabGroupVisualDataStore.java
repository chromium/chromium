// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.chromium.base.ThreadUtils.assertOnUiThread;

import android.content.Context;
import android.content.SharedPreferences;
import android.text.TextUtils;

import org.chromium.base.ContextUtils;
import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabGroupCollectionData;
import org.chromium.components.tab_groups.TabGroupColorId;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * Helper class to handle persistence of tab group metadata. This includes the title, color, and
 * collapsed state. This is not intended to be used directly. All access should route through the
 * {@link TabGroupModelFilter}.
 */
@NullMarked
public class TabGroupVisualDataStore {
    private static final String TAB_GROUP_TITLES_FILE_NAME = "tab_group_titles";
    private static final String TAB_GROUP_COLLAPSED_FILE_NAME = "tab_group_collapsed";
    private static final String TAB_GROUP_COLORS_FILE_NAME = "tab_group_colors";
    private static final String TAB_GROUP_TITLES_TOKEN_FILE_NAME = "tab_group_titles_token";
    private static final String TAB_GROUP_COLLAPSED_TOKEN_FILE_NAME = "tab_group_collapsed_token";
    private static final String TAB_GROUP_COLORS_TOKEN_FILE_NAME = "tab_group_colors_token";
    private static final String COLOR_INITIAL_MIGRATION_CHECK = "migration_check";
    private static final int COLOR_INITIAL_MIGRATION_NOT_DONE = 0;
    private static final int COLOR_INITIAL_MIGRATION_DONE = 1;
    private static final Map<Token, TabGroupCollectionData> sGroupsCache = new HashMap<>();

    /**
     * Deletes all the data for keys not in {@code tabGroupTokenIdStrings}. This should only be
     * performed synchronously on the UI thread with an exhaustive list of in-use tab group ids from
     * all tab group models.
     *
     * @param tabGroupTokenIdStrings The set of all the stringified {@link Token} tab group ids that
     *     are known about.
     */
    public static void deleteTabGroupDataExcluding(Set<String> tabGroupTokenIdStrings) {
        assertOnUiThread();
        deleteTabGroupDataExcludingForSharedPreference(
                getTokenTitleSharedPreferences(), tabGroupTokenIdStrings);
        deleteTabGroupDataExcludingForSharedPreference(
                getTokenColorSharedPreferences(), tabGroupTokenIdStrings);
        deleteTabGroupDataExcludingForSharedPreference(
                getTokenCollapsedSharedPreferences(), tabGroupTokenIdStrings);
    }

    private static void deleteTabGroupDataExcludingForSharedPreference(
            SharedPreferences prefs, Set<String> tabGroupIds) {
        SharedPreferences.Editor editor = prefs.edit();
        if (tabGroupIds.isEmpty()) {
            editor.clear().apply();
            return;
        }
        Set<String> orphanedKeys = new HashSet<>(prefs.getAll().keySet());
        orphanedKeys.removeAll(tabGroupIds);
        for (String key : orphanedKeys) {
            editor.remove(key);
        }
        editor.apply();
    }

    // Root ID methods.

    /**
     * This method stores tab group title with reference to {@code tabRootId}. Package protected as
     * all access should route through the {@link TabGroupModelFilter}.
     *
     * @param tabRootId The tab root ID which is used as reference to store group title.
     * @param title The tab group title to store.
     */
    /* package */ static void storeTabGroupTitle(int tabRootId, @Nullable String title) {
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
    /* package */ static void deleteTabGroupTitle(int tabRootId) {
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
    /* package */ static @Nullable String getTabGroupTitle(int tabRootId) {
        assert tabRootId != Tab.INVALID_TAB_ID;
        // TODO(crbug.com/40895368): Consider checking if this looks like the default plural string
        // and deleting and returning null if any users have saved tab group titles.
        return getTitleSharedPreferences().getString(String.valueOf(tabRootId), null);
    }

    private static SharedPreferences getTitleSharedPreferences() {
        return ContextUtils.getApplicationContext()
                .getSharedPreferences(TAB_GROUP_TITLES_FILE_NAME, Context.MODE_PRIVATE);
    }

    /**
     * Returns whether the initial migration of tab group colors has been done.
     *
     * @return Whether the initial migration of tab group colors has been done.
     */
    /* package */ static boolean isColorInitialMigrationDone() {
        return getColorSharedPreferences()
                        .getInt(COLOR_INITIAL_MIGRATION_CHECK, COLOR_INITIAL_MIGRATION_NOT_DONE)
                == COLOR_INITIAL_MIGRATION_DONE;
    }

    /** This method sets the initial migration of tab group colors as done. */
    /* package */ static void setColorInitialMigrationDone() {
        getColorSharedPreferences()
                .edit()
                .putInt(COLOR_INITIAL_MIGRATION_CHECK, COLOR_INITIAL_MIGRATION_DONE)
                .apply();
    }

    /**
     * This method stores tab group colors with reference to {@code tabRootId}. Package protected as
     * all access should route through the {@link TabGroupModelFilter}.
     *
     * @param tabRootId The tab root ID which is used as a reference to store group colors.
     * @param color The tab group color {@link TabGroupColorId} to store.
     */
    /* package */ static void storeTabGroupColor(int tabRootId, int color) {
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
    /* package */ static void deleteTabGroupColor(int tabRootId) {
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
    /* package */ static int getTabGroupColor(int tabRootId) {
        assert tabRootId != Tab.INVALID_TAB_ID;
        return getColorSharedPreferences()
                .getInt(String.valueOf(tabRootId), TabGroupColorUtils.INVALID_COLOR_ID);
    }

    private static SharedPreferences getColorSharedPreferences() {
        return ContextUtils.getApplicationContext()
                .getSharedPreferences(TAB_GROUP_COLORS_FILE_NAME, Context.MODE_PRIVATE);
    }

    /**
     * This method stores the collapsed state of a tab group with reference to {@code tabRootId}.
     *
     * @param tabRootId The tab root ID whose related group's collapsed state will be stored.
     * @param isCollapsed If the tab group is collapsed or expanded.
     */
    /* package */ static void storeTabGroupCollapsed(int tabRootId, boolean isCollapsed) {
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
     * This method deletes the collapsed state of a tab group with reference to {@code tabRootId}.
     *
     * @param tabRootId The tab root ID whose related tab group collapsed state will be deleted.
     */
    /* package */ static void deleteTabGroupCollapsed(int tabRootId) {
        assert tabRootId != Tab.INVALID_TAB_ID;
        getCollapsedSharedPreferences().edit().remove(String.valueOf(tabRootId)).apply();
    }

    /**
     * This method fetches the collapsed state of a tab group with reference to {@code tabRootId}.
     *
     * @param tabRootId The tab root ID whose related group's collapsed state will be fetched.
     * @return Whether the tab group is collapsed or expanded.
     */
    /* package */ static boolean getTabGroupCollapsed(int tabRootId) {
        assert tabRootId != Tab.INVALID_TAB_ID;
        return getCollapsedSharedPreferences().getBoolean(String.valueOf(tabRootId), false);
    }

    private static SharedPreferences getCollapsedSharedPreferences() {
        return ContextUtils.getApplicationContext()
                .getSharedPreferences(TAB_GROUP_COLLAPSED_FILE_NAME, Context.MODE_PRIVATE);
    }

    // Token methods.

    /**
     * This method stores tab group title with reference to {@code tabGroupId}.
     *
     * @param tabGroupId The tab group ID which is used as reference to store group title.
     * @param title The tab group title to store.
     */
    /* package */ static void storeTabGroupTitle(Token tabGroupId, @Nullable String title) {
        if (TextUtils.isEmpty(title)) {
            deleteTabGroupTitle(tabGroupId);
        } else {
            getTokenTitleSharedPreferences().edit().putString(tabGroupId.toString(), title).apply();
        }
    }

    /**
     * This method deletes a specific stored tab group title with reference to {@code tabGroupId}.
     *
     * @param tabGroupId The tab group ID whose related tab group title will be deleted.
     */
    /* package */ static void deleteTabGroupTitle(Token tabGroupId) {
        getTokenTitleSharedPreferences().edit().remove(tabGroupId.toString()).apply();
    }

    /**
     * This method fetches a tab group title with the related tab group ID.
     *
     * @param tabGroupId The tab group ID whose related tab group title will be fetched.
     * @return The stored title of the target tab group, default value is null. If the group is
     *     present in the cache, data will be read from there first.
     */
    /* package */ static @Nullable String getTabGroupTitle(Token tabGroupId) {
        if (sGroupsCache.containsKey(tabGroupId)) {
            TabGroupCollectionData groupCollectionData = sGroupsCache.get(tabGroupId);
            return groupCollectionData.getTitle();
        }
        return getTokenTitleSharedPreferences().getString(tabGroupId.toString(), null);
    }

    private static SharedPreferences getTokenTitleSharedPreferences() {
        return ContextUtils.getApplicationContext()
                .getSharedPreferences(TAB_GROUP_TITLES_TOKEN_FILE_NAME, Context.MODE_PRIVATE);
    }

    /**
     * This method stores a tab group color with reference to {@code tabGroupId}.
     *
     * @param tabGroupId The tab group ID which is used as a reference to store group colors.
     * @param color The tab group color {@link TabGroupColorId} to store.
     */
    /* package */ static void storeTabGroupColor(Token tabGroupId, int color) {
        getTokenColorSharedPreferences().edit().putInt(tabGroupId.toString(), color).apply();
    }

    /**
     * This method deletes a specific stored tab group color with reference to {@code tabGroupId}.
     *
     * @param tabGroupId The tab group ID whose related tab group color will be deleted.
     */
    /* package */ static void deleteTabGroupColor(Token tabGroupId) {
        getTokenColorSharedPreferences().edit().remove(tabGroupId.toString()).apply();
    }

    /**
     * This method fetches a tab group color for the related tab group ID.
     *
     * @param tabGroupId The tab group ID whose related tab group color will be fetched.
     * @return The stored color of the target tab group, default value is -1 (INVALID_COLOR_ID). If
     *     the group is present in the cache, data will be read from there first.
     */
    /* package */ static int getTabGroupColor(Token tabGroupId) {
        if (sGroupsCache.containsKey(tabGroupId)) {
            return sGroupsCache.get(tabGroupId).getColor();
        }
        return getTokenColorSharedPreferences()
                .getInt(tabGroupId.toString(), TabGroupColorUtils.INVALID_COLOR_ID);
    }

    private static SharedPreferences getTokenColorSharedPreferences() {
        return ContextUtils.getApplicationContext()
                .getSharedPreferences(TAB_GROUP_COLORS_TOKEN_FILE_NAME, Context.MODE_PRIVATE);
    }

    /**
     * This method stores the collapsed state of a tab group with reference to {@code tabGroupId}.
     *
     * @param tabGroupId The tab group ID whose related group's collapsed state will be stored.
     * @param isCollapsed If the tab group is collapsed or expanded.
     */
    /* package */ static void storeTabGroupCollapsed(Token tabGroupId, boolean isCollapsed) {
        if (isCollapsed) {
            getTokenCollapsedSharedPreferences()
                    .edit()
                    .putBoolean(tabGroupId.toString(), true)
                    .apply();
        } else {
            deleteTabGroupCollapsed(tabGroupId);
        }
    }

    /**
     * This method deletes the collapsed state of a tab group with reference to {@code tabGroupId}.
     *
     * @param tabGroupId The tab group ID whose related tab group collapsed state will be deleted.
     */
    /* package */ static void deleteTabGroupCollapsed(Token tabGroupId) {
        getTokenCollapsedSharedPreferences().edit().remove(tabGroupId.toString()).apply();
    }

    /**
     * This method fetches the collapsed state of a tab group with reference to {@code tabGroupId}.
     *
     * @param tabGroupId The tab group ID whose related group's collapsed state will be fetched.
     * @return Whether the tab group is collapsed or expanded. If the group is present in the cache,
     *     data will be read from there first.
     */
    /* package */ static boolean getTabGroupCollapsed(Token tabGroupId) {
        if (sGroupsCache.containsKey(tabGroupId)) {
            return sGroupsCache.get(tabGroupId).isCollapsed();
        }
        return getTokenCollapsedSharedPreferences().getBoolean(tabGroupId.toString(), false);
    }

    private static SharedPreferences getTokenCollapsedSharedPreferences() {
        return ContextUtils.getApplicationContext()
                .getSharedPreferences(TAB_GROUP_COLLAPSED_TOKEN_FILE_NAME, Context.MODE_PRIVATE);
    }

    /**
     * Deletes all visual data associated with a given tab group ID.
     *
     * @param tabGroupId The identifier for the tab group.
     */
    /* package */ static void deleteAllVisualDataForGroup(Token tabGroupId) {
        deleteTabGroupTitle(tabGroupId);
        deleteTabGroupColor(tabGroupId);
        deleteTabGroupCollapsed(tabGroupId);
    }

    /**
     * Caches a list of tab group visual data. This data is higher priority to, and will be fetched
     * prior to data in SharedPrefs.
     *
     * @param groups An array of {@link TabGroupCollectionData} objects representing the tab groups
     *     to cache.
     */
    public static void cacheGroups(TabGroupCollectionData[] groups) {
        for (TabGroupCollectionData data : groups) {
            sGroupsCache.put(data.getTabGroupId(), data);
        }
    }

    /**
     * Removes the associated tab group data from the list of cached groups, if present.
     *
     * @param groups An array of {@link TabGroupCollectionData} objects representing the cached tab
     *     groups to remove.
     */
    public static void removeCachedGroups(TabGroupCollectionData[] groups) {
        for (TabGroupCollectionData data : groups) {
            sGroupsCache.remove(data.getTabGroupId());
        }
    }

    // Migration methods.

    /**
     * Migrates all visual data from root ID-based storage to token-based storage for a given tab
     * group.
     *
     * @param rootId The root ID of the tab group.
     * @param tabGroupId The token identifier for the tab group.
     */
    /* package */ static void migrateToTokenKeyedStorage(int rootId, Token tabGroupId) {
        String title = getTabGroupTitle(rootId);
        if (title != null) {
            storeTabGroupTitle(tabGroupId, title);
            deleteTabGroupTitle(rootId);
        }

        int color = getTabGroupColor(rootId);
        if (color != TabGroupColorUtils.INVALID_COLOR_ID) {
            storeTabGroupColor(tabGroupId, color);
            deleteTabGroupColor(rootId);
        }

        boolean isCollapsed = getTabGroupCollapsed(rootId);
        if (isCollapsed) {
            storeTabGroupCollapsed(tabGroupId, true);
            deleteTabGroupCollapsed(rootId);
        }
    }

    /**
     * Migrates all visual data from token-based storage to root ID-based storage for a given tab
     * group.
     *
     * @param tabGroupId The token identifier for the tab group.
     * @param rootId The root ID of the tab group.
     */
    /* package */ static void migrateFromTokenKeyedStorage(Token tabGroupId, int rootId) {
        String title = getTabGroupTitle(tabGroupId);
        if (title != null) {
            storeTabGroupTitle(rootId, title);
            deleteTabGroupTitle(tabGroupId);
        }

        int color = getTabGroupColor(tabGroupId);
        if (color != TabGroupColorUtils.INVALID_COLOR_ID) {
            storeTabGroupColor(rootId, color);
            deleteTabGroupColor(tabGroupId);
        }

        boolean isCollapsed = getTabGroupCollapsed(tabGroupId);
        if (isCollapsed) {
            storeTabGroupCollapsed(rootId, true);
            deleteTabGroupCollapsed(tabGroupId);
        }
    }
}
