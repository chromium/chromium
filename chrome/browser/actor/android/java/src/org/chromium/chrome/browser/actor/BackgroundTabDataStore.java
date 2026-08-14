// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import android.content.Context;
import android.content.SharedPreferences;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;

/** Persists properties related to BackgroundTabData in SharedPreferences. */
@NullMarked
public class BackgroundTabDataStore {

    private static final String BACKGROUND_TAB_DATA_FILE_NAME = "background_tab_data";

    private static final String KEY_PLACEHOLDER_TAB_ID = "placeholder_tab_id_";
    private static final String KEY_ORIGINAL_TAB_INDEX = "original_tab_index_";
    private static final String KEY_TAB_WINDOW_ID = "tab_window_id_";
    private static final String KEY_SHOULD_READ = "should_read_";

    private static SharedPreferences getSharedPreferences() {
        return ContextUtils.getApplicationContext()
                .getSharedPreferences(BACKGROUND_TAB_DATA_FILE_NAME, Context.MODE_PRIVATE);
    }

    /**
     * Stores the placeholder tab ID associated with an original tab ID.
     *
     * @param originalTabId The ID of the original tab.
     * @param placeholderTabId The placeholder tab ID to store.
     */
    public static void storePlaceholderTabId(int originalTabId, int placeholderTabId) {
        getSharedPreferences()
                .edit()
                .putInt(KEY_PLACEHOLDER_TAB_ID + originalTabId, placeholderTabId)
                .apply();
    }

    /**
     * Deletes the stored placeholder tab ID associated with an original tab ID.
     *
     * @param originalTabId The ID of the original tab.
     */
    public static void deletePlaceholderTabId(int originalTabId) {
        getSharedPreferences().edit().remove(KEY_PLACEHOLDER_TAB_ID + originalTabId).apply();
    }

    /**
     * Retrieves the placeholder tab ID associated with an original tab ID.
     *
     * @param originalTabId The ID of the original tab.
     * @return The stored placeholder tab ID, or {@link Tab#INVALID_TAB_ID} if not found.
     */
    public static int getPlaceholderTabId(int originalTabId) {
        return getSharedPreferences()
                .getInt(KEY_PLACEHOLDER_TAB_ID + originalTabId, Tab.INVALID_TAB_ID);
    }

    /**
     * Stores the original tab index associated with an original tab ID.
     *
     * @param originalTabId The ID of the original tab.
     * @param originalTabIndex The original tab index to store.
     */
    public static void storeOriginalTabIndex(int originalTabId, int originalTabIndex) {
        getSharedPreferences()
                .edit()
                .putInt(KEY_ORIGINAL_TAB_INDEX + originalTabId, originalTabIndex)
                .apply();
    }

    /**
     * Deletes the stored original tab index associated with an original tab ID.
     *
     * @param originalTabId The ID of the original tab.
     */
    public static void deleteOriginalTabIndex(int originalTabId) {
        getSharedPreferences().edit().remove(KEY_ORIGINAL_TAB_INDEX + originalTabId).apply();
    }

    /**
     * Retrieves the original tab index associated with an original tab ID.
     *
     * @param originalTabId The ID of the original tab.
     * @return The stored original tab index, or {@link TabModel#INVALID_TAB_INDEX} if not found.
     */
    public static int getOriginalTabIndex(int originalTabId) {
        return getSharedPreferences()
                .getInt(KEY_ORIGINAL_TAB_INDEX + originalTabId, TabModel.INVALID_TAB_INDEX);
    }

    /**
     * Stores the tab window ID associated with an original tab ID.
     *
     * @param originalTabId The ID of the original tab.
     * @param tabWindowId The tab window ID to store.
     */
    public static void storeTabWindowId(int originalTabId, int tabWindowId) {
        getSharedPreferences()
                .edit()
                .putInt(KEY_TAB_WINDOW_ID + originalTabId, tabWindowId)
                .apply();
    }

    /**
     * Deletes the stored tab window ID associated with an original tab ID.
     *
     * @param originalTabId The ID of the original tab.
     */
    public static void deleteTabWindowId(int originalTabId) {
        getSharedPreferences().edit().remove(KEY_TAB_WINDOW_ID + originalTabId).apply();
    }

    /**
     * Retrieves the tab window ID associated with an original tab ID.
     *
     * @param originalTabId The ID of the original tab.
     * @return The stored tab window ID, or -1 if not found.
     */
    public static int getTabWindowId(int originalTabId) {
        return getSharedPreferences().getInt(KEY_TAB_WINDOW_ID + originalTabId, -1);
    }

    /**
     * Stores whether the background tab property should be read.
     *
     * @param originalTabId The ID of the original tab.
     * @param shouldRead Whether the property should be read.
     */
    public static void storeShouldRead(int originalTabId, boolean shouldRead) {
        getSharedPreferences()
                .edit()
                .putBoolean(KEY_SHOULD_READ + originalTabId, shouldRead)
                .apply();
    }

    /**
     * Deletes the stored read flag associated with an original tab ID.
     *
     * @param originalTabId The ID of the original tab.
     */
    public static void deleteShouldRead(int originalTabId) {
        getSharedPreferences().edit().remove(KEY_SHOULD_READ + originalTabId).apply();
    }

    /**
     * Retrieves whether the background tab property should be read.
     *
     * @param originalTabId The ID of the original tab.
     * @param defaultValue The default boolean to return if not found.
     * @return The stored read flag, or {@code defaultValue} if not found.
     */
    public static boolean getShouldRead(int originalTabId, boolean defaultValue) {
        return getSharedPreferences().getBoolean(KEY_SHOULD_READ + originalTabId, defaultValue);
    }

    /**
     * Convenience method to persist all properties of a BackgroundTabData object and mark it for
     * reading.
     *
     * @param tabData The BackgroundTabData to be persisted.
     */
    public static void saveBackgroundTabData(BackgroundSession.BackgroundTabData tabData) {
        int originalTabId = tabData.getTab().getId();
        SharedPreferences.Editor editor = getSharedPreferences().edit();

        if (tabData.getPlaceholderTabId() != null) {
            editor.putInt(KEY_PLACEHOLDER_TAB_ID + originalTabId, tabData.getPlaceholderTabId());
        } else {
            editor.remove(KEY_PLACEHOLDER_TAB_ID + originalTabId);
        }

        if (tabData.getOriginalTabIndex() != TabModel.INVALID_TAB_INDEX) {
            editor.putInt(KEY_ORIGINAL_TAB_INDEX + originalTabId, tabData.getOriginalTabIndex());
        } else {
            editor.remove(KEY_ORIGINAL_TAB_INDEX + originalTabId);
        }

        if (tabData.getTabWindowId() != -1) {
            editor.putInt(KEY_TAB_WINDOW_ID + originalTabId, tabData.getTabWindowId());
        } else {
            editor.remove(KEY_TAB_WINDOW_ID + originalTabId);
        }

        editor.putBoolean(KEY_SHOULD_READ + originalTabId, true);
        editor.apply();
    }

    /**
     * Removes all background tab data associated with an original tab ID.
     *
     * @param originalTabId The ID of the original tab.
     */
    public static void removeBackgroundTabData(int originalTabId) {
        getSharedPreferences()
                .edit()
                .remove(KEY_PLACEHOLDER_TAB_ID + originalTabId)
                .remove(KEY_ORIGINAL_TAB_INDEX + originalTabId)
                .remove(KEY_TAB_WINDOW_ID + originalTabId)
                .remove(KEY_SHOULD_READ + originalTabId)
                .apply();
    }

    /** Deletes the entire SharedPreferences file, providing a safe rollback path. */
    public static void clearAllBackgroundTabData() {
        ContextUtils.getApplicationContext().deleteSharedPreferences(BACKGROUND_TAB_DATA_FILE_NAME);
    }
}
