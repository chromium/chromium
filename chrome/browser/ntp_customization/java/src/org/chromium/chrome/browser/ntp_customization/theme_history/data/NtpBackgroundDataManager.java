// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_history.data;

import androidx.annotation.VisibleForTesting;

import org.json.JSONArray;
import org.json.JSONException;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.theme_history.data.NtpBackgroundDataBase.PlatformType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

import java.util.ArrayList;
import java.util.List;

/** Centralizes management of NTP background preference data. */
@NullMarked
public class NtpBackgroundDataManager {
    private static final int MAXIMUM_LOCAL_HISTORY = 3;
    private static final int MAXIMUM_REMOTE_HISTORY = 2;

    public NtpBackgroundDataManager() {}

    /**
     * Saves the NTP's background types from cross device sync to the shared preference.
     *
     * @param backgroundDataList The list of background data to save.
     * @throws JSONException If there is an error converting the data to JSON.
     */
    public void saveRemoteSyncDataToSharedPreference(List<NtpBackgroundDataBase> backgroundDataList)
            throws JSONException {
        for (NtpBackgroundDataBase data : backgroundDataList) {
            if (data.getPlatformType() <= PlatformType.ANDROID_LOCAL) continue;
            saveRemoteSyncDataToSharedPreference(data);
        }
    }

    /**
     * Saves a single NTP's background type from cross device sync to the shared preference.
     *
     * @param backgroundData The background data to save.
     * @throws JSONException If there is an error converting the data to JSON.
     */
    public void saveRemoteSyncDataToSharedPreference(NtpBackgroundDataBase backgroundData)
            throws JSONException {
        @PlatformType int platformType = backgroundData.getPlatformType();
        List<NtpBackgroundDataBase> currentList =
                getBackgroundDataListFromSharedPreference(platformType);
        JSONArray newList = new JSONArray();

        if (currentList == null || currentList.isEmpty()) {
            newList.put(backgroundData.toJson());
            writeToShardPreference(newList, platformType);
            return;
        }

        // To update existing remote sync data:
        // If this backgroundData already in the current remote sync data list, moves it to the
        // first one. Otherwise, adds it as the first one on the list and removed the last data of
        // the list if exceeds the maximum allowed size of history data.
        int index = currentList.indexOf(backgroundData);
        if (index != -1) {
            currentList.remove(index);
        } else {
            int length = currentList.size();
            if (length >= MAXIMUM_REMOTE_HISTORY) {
                currentList.remove(currentList.size() - 1);
            }
        }
        currentList.add(0, backgroundData);

        for (var data : currentList) {
            newList.put(data.toJson());
        }
        writeToShardPreference(newList, platformType);
    }

    /**
     * Saves the user selected background type to the shared preference for local history.
     *
     * @param backgroundData The user selected background data.
     * @throws JSONException If there is an error converting the data to JSON.
     */
    public void saveUserSelectedBackgroundTypeToSharedPreference(
            NtpBackgroundDataBase backgroundData) throws JSONException {
        @PlatformType int platformTypeToSave = PlatformType.ANDROID_LOCAL;
        List<NtpBackgroundDataBase> currentList =
                getBackgroundDataListFromSharedPreference(platformTypeToSave);
        JSONArray newList = new JSONArray();

        if (currentList == null || currentList.isEmpty()) {
            newList.put(backgroundData.toJson());
            writeToShardPreference(newList, platformTypeToSave);
            return;
        }

        // To update user selected background history data:
        // If the user chose a cross device synced background type, we add the type to the local
        // selection history list, but remove any existing type from that platform from the local
        // selection history. This allows to cache only the latest chosen background type from any
        // remote platform.
        int platformTypeOfNewData = backgroundData.getPlatformType();
        if (platformTypeOfNewData != PlatformType.ANDROID_LOCAL) {
            currentList.removeIf(item -> item.getPlatformType() == platformTypeOfNewData);
        }
        currentList.add(0, backgroundData);
        int size = currentList.size();
        if (size > MAXIMUM_LOCAL_HISTORY) {
            currentList.remove(size - 1);
        }
        for (var data : currentList) {
            newList.put(data.toJson());
        }
        writeToShardPreference(newList, platformTypeToSave);
    }

    /**
     * Returns the saved NTP's background history data of the given platform type as a list of
     * {@link NtpBackgroundDataBase}.
     *
     * @param platformType The platform type to get the background data for.
     * @return The background data for the given platform type.
     * @throws JSONException If the background data is not a valid JSON array.
     */
    @VisibleForTesting
    @Nullable List<NtpBackgroundDataBase> getBackgroundDataListFromSharedPreference(
            @PlatformType int platformType) throws JSONException {
        JSONArray historyDataArray = getJsonArrayFromSharedPreferenceImpl(platformType);
        if (historyDataArray == null) return null;

        List<NtpBackgroundDataBase> backgroundDataList = new ArrayList<>(historyDataArray.length());
        for (int i = 0; i < historyDataArray.length(); i++) {
            NtpBackgroundDataBase data =
                    NtpBackgroundDataUtils.fromJson(historyDataArray.getJSONObject(i));
            if (data != null) {
                backgroundDataList.add(data);
            }
        }
        return backgroundDataList;
    }

    /**
     * Returns the saved NTP's background sync data of the given platform type as a {@link
     * JSONArray}.
     *
     * @param platformType The platform type to get the background data for.
     * @return The background data for the given platform type.
     * @throws JSONException If the background data is not a valid JSON array.
     */
    @Nullable JSONArray getJsonArrayFromSharedPreferenceImpl(@PlatformType int platformType)
            throws JSONException {
        String historyData =
                ChromeSharedPreferences.getInstance()
                        .readString(getSharedPreferenceKey(platformType), null);
        if (historyData == null || historyData.isEmpty()) return null;

        return new JSONArray(historyData);
    }

    /**
     * Writes the given {@link JSONArray} to shared preferences for the specified platform type.
     *
     * @param current The JSON array to write.
     * @param platformType The platform type associated with the data.
     */
    private void writeToShardPreference(JSONArray current, @PlatformType int platformType) {
        String key = getSharedPreferenceKey(platformType);
        ChromeSharedPreferences.getInstance().writeString(key, current.toString());
    }

    /**
     * Returns the shared preference key for the given platform type.
     *
     * @param platformType The platform type.
     * @return The shared preference key string.
     */
    private String getSharedPreferenceKey(@PlatformType int platformType) {
        return ChromePreferenceKeys.NTP_CUSTOMIZATION_SYNC_HISTORY_DATA.createKey(platformType);
    }

    /** Resets the shared preferences used by this manager for testing purposes. */
    public void resetSharedPreferenceForTesting() {
        SharedPreferencesManager sharedPreferencesManager = ChromeSharedPreferences.getInstance();
        for (int i = PlatformType.ANDROID_LOCAL; i < PlatformType.MAX_COUNT; i++) {
            sharedPreferencesManager.removeKey(getSharedPreferenceKey(i));
        }
    }
}
