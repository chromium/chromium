// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.chromium.base.ThreadUtils.assertOnUiThread;
import static org.chromium.chrome.browser.tabmodel.TabGroupTitleUtils.UNSET_TAB_GROUP_TITLE;
import static org.chromium.chrome.browser.tabmodel.TabGroupTitleUtils.isTitleUnset;

import android.content.Context;
import android.content.SharedPreferences;

import org.chromium.base.ContextUtils;
import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
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
 * {@link TabModel}.
 */
@NullMarked
public class TabGroupVisualDataStore {
    static final String TAB_GROUP_TITLES_FILE_NAME = "tab_group_titles";
    static final String TAB_GROUP_COLLAPSED_FILE_NAME = "tab_group_collapsed";
    static final String TAB_GROUP_COLORS_FILE_NAME = "tab_group_colors";
    private static final String TAB_GROUP_TITLES_TOKEN_FILE_NAME = "tab_group_titles_token";
    private static final String TAB_GROUP_COLLAPSED_TOKEN_FILE_NAME = "tab_group_collapsed_token";
    private static final String TAB_GROUP_COLORS_TOKEN_FILE_NAME = "tab_group_colors_token";
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

    /**
     * This method checks if visual data for the specified tab group is currently present in the
     * in-memory cache, populated during tab restore.
     *
     * <p>This is used to determine if a storage update is required to flush potentially dirty
     * cached data to SharedPreferences, even if the incoming value matches the cached value.
     *
     * <p>Package protected as all access should route through the {@link TabModel}.
     *
     * @param tabGroupId The token identifier for the tab group.
     * @return True if the tab group data is currently cached, false otherwise.
     */
    /* package */ static boolean isTabGroupCachedForRestore(Token tabGroupId) {
        return sGroupsCache.containsKey(tabGroupId);
    }

    /**
     * Stores the tab group title associated with {@code tabGroupId}.
     *
     * @param tabGroupId The identifier for the tab group.
     * @param title The tab group title to store.
     */
    /* package */ static void storeTabGroupTitle(Token tabGroupId, String title) {
        flushCachedData(tabGroupId);
        if (isTitleUnset(title)) {
            deleteTabGroupTitle(tabGroupId);
        } else {
            getTokenTitleSharedPreferences().edit().putString(tabGroupId.toString(), title).apply();
        }
    }

    /**
     * Deletes the stored tab group title associated with {@code tabGroupId}.
     *
     * @param tabGroupId The identifier for the tab group.
     */
    /* package */ static void deleteTabGroupTitle(Token tabGroupId) {
        flushCachedData(tabGroupId);
        getTokenTitleSharedPreferences().edit().remove(tabGroupId.toString()).apply();
    }

    /**
     * Fetches the tab group title associated with {@code tabGroupId}.
     *
     * @param tabGroupId The identifier for the tab group.
     * @return The stored title of the target tab group, default value is {@link
     *     TabGroupTitleUtils#UNSET_TAB_GROUP_TITLE}. If the group is present in the cache, data
     *     will be read from there first.
     */
    /* package */ static String getTabGroupTitle(Token tabGroupId) {
        TabGroupCollectionData groupCollectionData = sGroupsCache.get(tabGroupId);
        if (groupCollectionData != null) {
            return groupCollectionData.getTitle();
        }
        return getTokenTitleSharedPreferences()
                .getString(tabGroupId.toString(), UNSET_TAB_GROUP_TITLE);
    }

    private static SharedPreferences getTokenTitleSharedPreferences() {
        return ContextUtils.getApplicationContext()
                .getSharedPreferences(TAB_GROUP_TITLES_TOKEN_FILE_NAME, Context.MODE_PRIVATE);
    }

    /**
     * Stores the tab group color associated with {@code tabGroupId}.
     *
     * @param tabGroupId The identifier for the tab group.
     * @param color The tab group color {@link TabGroupColorId} to store.
     */
    /* package */ static void storeTabGroupColor(Token tabGroupId, @TabGroupColorId int color) {
        flushCachedData(tabGroupId);
        getTokenColorSharedPreferences().edit().putInt(tabGroupId.toString(), color).apply();
    }

    /**
     * Deletes the stored tab group color associated with {@code tabGroupId}.
     *
     * @param tabGroupId The identifier for the tab group.
     */
    /* package */ static void deleteTabGroupColor(Token tabGroupId) {
        flushCachedData(tabGroupId);
        getTokenColorSharedPreferences().edit().remove(tabGroupId.toString()).apply();
    }

    /**
     * Fetches the tab group color associated with {@code tabGroupId}.
     *
     * @param tabGroupId The identifier for the tab group.
     * @return The stored color of the target tab group, default value is {@link
     *     TabGroupColorUtils#INVALID_COLOR_ID}. If the group is present in the cache, data will be
     *     read from there first.
     */
    /* package */ static int getTabGroupColor(Token tabGroupId) {
        TabGroupCollectionData groupCollectionData = sGroupsCache.get(tabGroupId);
        if (groupCollectionData != null) {
            return groupCollectionData.getColor();
        }
        return getTokenColorSharedPreferences()
                .getInt(tabGroupId.toString(), TabGroupColorUtils.INVALID_COLOR_ID);
    }

    private static SharedPreferences getTokenColorSharedPreferences() {
        return ContextUtils.getApplicationContext()
                .getSharedPreferences(TAB_GROUP_COLORS_TOKEN_FILE_NAME, Context.MODE_PRIVATE);
    }

    /**
     * Stores the collapsed state of the tab group associated with {@code tabGroupId}.
     *
     * @param tabGroupId The identifier for the tab group.
     * @param isCollapsed If the tab group is collapsed or expanded.
     */
    /* package */ static void storeTabGroupCollapsed(Token tabGroupId, boolean isCollapsed) {
        flushCachedData(tabGroupId);
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
     * Deletes the collapsed state of the tab group associated with {@code tabGroupId}.
     *
     * @param tabGroupId The identifier for the tab group.
     */
    /* package */ static void deleteTabGroupCollapsed(Token tabGroupId) {
        flushCachedData(tabGroupId);
        getTokenCollapsedSharedPreferences().edit().remove(tabGroupId.toString()).apply();
    }

    /**
     * Fetches the collapsed state of the tab group associated with {@code tabGroupId}.
     *
     * @param tabGroupId The identifier for the tab group.
     * @return Whether the tab group is collapsed or expanded. If the group is present in the cache,
     *     data will be read from there first.
     */
    /* package */ static boolean getTabGroupCollapsed(Token tabGroupId) {
        TabGroupCollectionData groupCollectionData = sGroupsCache.get(tabGroupId);
        if (groupCollectionData != null) {
            return groupCollectionData.isCollapsed();
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
        assert rootId != Tab.INVALID_TAB_ID;
        Context context = ContextUtils.getApplicationContext();
        String rootIdKey = String.valueOf(rootId);

        SharedPreferences titlePrefs =
                context.getSharedPreferences(TAB_GROUP_TITLES_FILE_NAME, Context.MODE_PRIVATE);
        String title = titlePrefs.getString(rootIdKey, UNSET_TAB_GROUP_TITLE);
        if (!isTitleUnset(title)) {
            storeTabGroupTitle(tabGroupId, title);
            titlePrefs.edit().remove(rootIdKey).apply();
        }

        SharedPreferences colorPrefs =
                context.getSharedPreferences(TAB_GROUP_COLORS_FILE_NAME, Context.MODE_PRIVATE);
        int color = colorPrefs.getInt(rootIdKey, TabGroupColorUtils.INVALID_COLOR_ID);
        if (color != TabGroupColorUtils.INVALID_COLOR_ID) {
            storeTabGroupColor(tabGroupId, color);
            colorPrefs.edit().remove(rootIdKey).apply();
        }

        SharedPreferences collapsedPrefs =
                context.getSharedPreferences(TAB_GROUP_COLLAPSED_FILE_NAME, Context.MODE_PRIVATE);
        if (collapsedPrefs.getBoolean(rootIdKey, false)) {
            storeTabGroupCollapsed(tabGroupId, /* isCollapsed= */ true);
            collapsedPrefs.edit().remove(rootIdKey).apply();
        }
    }

    /**
     * Removes the group data from the memory cache and ensures all its properties are persisted to
     * SharedPreferences. This prevents data loss for properties not currently being updated when
     * the cache entry is invalidated.
     *
     * @param tabGroupId The token identifier for the tab group.
     */
    private static void flushCachedData(Token tabGroupId) {
        TabGroupCollectionData data = sGroupsCache.remove(tabGroupId);
        if (data == null) return;

        String tabGroupIdString = tabGroupId.toString();

        SharedPreferences.Editor titleEditor = getTokenTitleSharedPreferences().edit();
        String title = data.getTitle();
        if (isTitleUnset(title)) {
            titleEditor.remove(tabGroupIdString);
        } else {
            titleEditor.putString(tabGroupIdString, title);
        }
        titleEditor.apply();

        SharedPreferences.Editor colorEditor = getTokenColorSharedPreferences().edit();
        @TabGroupColorId int color = data.getColor();
        if (color == TabGroupColorUtils.INVALID_COLOR_ID) {
            colorEditor.remove(tabGroupIdString);
        } else {
            colorEditor.putInt(tabGroupIdString, color);
        }
        colorEditor.apply();

        SharedPreferences.Editor collapsedEditor = getTokenCollapsedSharedPreferences().edit();
        boolean isCollapsed = data.isCollapsed();
        if (isCollapsed) {
            collapsedEditor.putBoolean(tabGroupIdString, true);
        } else {
            collapsedEditor.remove(tabGroupIdString);
        }
        collapsedEditor.apply();
    }
}
