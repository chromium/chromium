// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.search;

import org.jni_zero.CalledByNative;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.Log;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;

import java.util.LinkedHashMap;
import java.util.Map;

/**
 * Keeps the latest preference settings chosen by users from search results. Duplicated entries are
 * removed, and the entries are ordered as they are inserted.
 */
@NullMarked
public class RecentSearchQueue extends LinkedHashMap<String, SettingsIndexData.Entry> {
    private static final String TAG = "RecentSearchQueue";
    private static final int MAX_SIZE = 3;

    private @Nullable static RecentSearchQueue sInstance;

    /** Returns {@link RecentSearchQueue} instance. */
    public static RecentSearchQueue getInstance() {
        if (sInstance == null) {
            sInstance = new RecentSearchQueue();
        }
        return sInstance;
    }

    /** Adds a new search entry to the recent search list. */
    public void add(SettingsIndexData.Entry entry) {
        put(entry.key, entry);
    }

    @Override
    protected boolean removeEldestEntry(Map.Entry<String, SettingsIndexData.Entry> eldest) {
        return size() > MAX_SIZE;
    }

    @CalledByNative
    public static void deleteDiskData() {
        if (!ChromeFeatureList.sSearchInSettings.isEnabled()) return;

        if (sInstance != null) sInstance.clear();
        SharedPreferencesManager preferencesManager = ChromeSharedPreferences.getInstance();
        preferencesManager.removeKey(ChromePreferenceKeys.SETTINGS_RECENT_SEARCH_ENTRIES);
    }

    /** Persist the recent entries to disk and reset cached data. */
    public void persistToDiskAndReset() {
        JSONArray jsonArray = new JSONArray();
        for (SettingsIndexData.Entry entry : values()) {
            var obj = entry.toJsonObject();
            if (obj != null) jsonArray.put(obj);
        }
        SharedPreferencesManager preferencesManager = ChromeSharedPreferences.getInstance();
        preferencesManager.writeString(
                ChromePreferenceKeys.SETTINGS_RECENT_SEARCH_ENTRIES, jsonArray.toString());
        clear();
        sInstance = null;
    }

    /**
     * Restore the recent search entries from disk.
     *
     * @throws IllegalArgumentException if there's an error retrieving the data.
     */
    public void restoreFromDisk() throws IllegalArgumentException {
        SharedPreferencesManager preferencesManager = ChromeSharedPreferences.getInstance();
        String data =
                preferencesManager.readString(
                        ChromePreferenceKeys.SETTINGS_RECENT_SEARCH_ENTRIES, "");
        JSONArray jsonArray;
        try {
            jsonArray = new JSONArray(data);
        } catch (JSONException e) {
            Log.e(TAG, "Error restoring recent search from a disk file");
            return;
        }
        for (int i = 0; i < jsonArray.length(); i++) {
            try {
                JSONObject obj = jsonArray.getJSONObject(i);
                var entry = SettingsIndexData.Entry.fromJson(obj);
                if (entry != null) add(entry);
            } catch (JSONException e) {
                Log.e(TAG, "Error restoring Entry from JSON object");
            }
        }
    }
}
