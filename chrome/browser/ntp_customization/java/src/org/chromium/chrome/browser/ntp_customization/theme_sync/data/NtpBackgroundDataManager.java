// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_sync.data;

import android.content.Context;

import org.json.JSONArray;
import org.json.JSONException;

import org.chromium.base.Log;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataBase.PlatformType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/** Centralizes management of NTP background preference data. */
@NullMarked
public class NtpBackgroundDataManager {
    private static final String TAG = "NtpBackgroundData";
    private static final int MAXIMUM_LOCAL_HISTORY = 3;
    private static final int MAXIMUM_REMOTE_HISTORY = 2;

    private final Context mContext;

    /**
     * @param context The application context.
     */
    public NtpBackgroundDataManager(Context context) {
        mContext = context;
    }

    /**
     * Saves the NTP's background types from cross device sync to the shared preference.
     * TODO(https://crbug.com/488439751): Saves the sync data in a background thread.
     *
     * @param backgroundDataGroup The group of background data to save.
     */
    public void saveRemoteSyncDataToSharedPreference(NtpBackgroundDataGroup backgroundDataGroup) {
        for (NtpBackgroundDataBase data : backgroundDataGroup) {
            if (data.getPlatformType() <= PlatformType.ANDROID_LOCAL) continue;
            saveRemoteSyncDataToSharedPreference(data);
        }
    }

    /**
     * Saves a single NTP's background type from cross device sync to the shared preference.
     *
     * @param backgroundData The background data to save.
     */
    public void saveRemoteSyncDataToSharedPreference(NtpBackgroundDataBase backgroundData) {
        try {
            @PlatformType int platformType = backgroundData.getPlatformType();
            NtpBackgroundDataGroup currentGroup =
                    getBackgroundDataGroupFromSharedPreference(platformType);

            if (currentGroup.isEmpty()) {
                JSONArray newList = new JSONArray();
                newList.put(backgroundData.toJson());
                writeToSharedPreference(newList, platformType);
                return;
            }

            // To update existing remote sync data:
            // If this backgroundData already in the current remote sync data list, moves it to the
            // first one. Otherwise, adds it as the first one on the list and removed the last data
            // of the list if exceeds the maximum allowed size of history data.
            int index = currentGroup.indexOf(backgroundData);
            if (index != -1) {
                currentGroup.remove(index);
            } else {
                if (currentGroup.size() >= MAXIMUM_REMOTE_HISTORY) {
                    currentGroup.remove(currentGroup.size() - 1);
                }
            }
            currentGroup.add(0, backgroundData);

            writeToSharedPreference(currentGroup.toJsonArray(), platformType);
        } catch (JSONException e) {
            Log.i(
                    TAG,
                    "Failed to save NTP's sync background data to the SharedPreference: platform"
                            + " type = %d, data type = %d.",
                    backgroundData.getPlatformType(),
                    backgroundData.getBackgroundType());
        }
    }

    /**
     * Saves the user selected background type to the shared preference for local history.
     *
     * @param backgroundData The user selected background data.
     */
    public void saveUserSelectedBackgroundTypeToSharedPreference(
            NtpBackgroundDataBase backgroundData) {
        try {
            @PlatformType int platformTypeToSave = PlatformType.ANDROID_LOCAL;
            NtpBackgroundDataGroup currentGroup =
                    getBackgroundDataGroupFromSharedPreference(platformTypeToSave);

            if (currentGroup.isEmpty()) {
                JSONArray newList = new JSONArray();
                newList.put(backgroundData.toJson());
                writeToSharedPreference(newList, platformTypeToSave);
                return;
            }

            // To update user selected background history data:
            // If the user chose a cross device synced background type, we add the type to the local
            // selection history list, but remove any existing type from that platform from the
            // local selection history. This allows to cache only the latest chosen background type
            // from any remote platform.
            int platformTypeOfNewData = backgroundData.getPlatformType();
            if (platformTypeOfNewData != PlatformType.ANDROID_LOCAL) {
                currentGroup.removeIf(item -> item.getPlatformType() == platformTypeOfNewData);
            }

            // If the backgroundData already in local history, removes the existing one.
            int index = currentGroup.indexOf(backgroundData);
            if (index != -1) {
                currentGroup.remove(index);
            }
            currentGroup.add(0, backgroundData);
            if (currentGroup.size() > MAXIMUM_LOCAL_HISTORY) {
                currentGroup.remove(currentGroup.size() - 1);
            }
            writeToSharedPreference(currentGroup.toJsonArray(), platformTypeToSave);
        } catch (JSONException e) {
            Log.i(
                    TAG,
                    "Failed to save user selected NTP's sync background data to the"
                            + " SharedPreference: data type = %d.",
                    backgroundData.getBackgroundType());
        }
    }

    /**
     * Returns the saved NTP's background history data of the given platform type as a list of
     * {@link NtpBackgroundDataBase}.
     *
     * @return The background data for the given platform type.
     */
    public NtpBackgroundDataGroup[] getBackgroundDataListFromSharedPreference() {
        NtpBackgroundDataGroup[] dataList = new NtpBackgroundDataGroup[PlatformType.MAX_COUNT];
        for (int i = PlatformType.ANDROID_LOCAL; i < PlatformType.MAX_COUNT; i++) {
            dataList[i] = getBackgroundDataGroupFromSharedPreference(i);
        }
        return dataList;
    }

    /**
     * Returns the saved NTP's background history data of the given platform type as a {@link
     * NtpBackgroundDataGroup}.
     *
     * @param platformType The platform type to get the background data for.
     * @return The background data for the given platform type.
     */
    public NtpBackgroundDataGroup getBackgroundDataGroupFromSharedPreference(
            @PlatformType int platformType) {
        JSONArray historyDataArray = getJsonArrayFromSharedPreferenceImpl(platformType);
        if (historyDataArray != null) {
            try {
                return NtpBackgroundDataGroup.fromJson(mContext, historyDataArray);
            } catch (JSONException e) {
                Log.i(
                        TAG,
                        "Failed to load NTP's sync background data from the SharedPreference:"
                                + " platform type = %d.",
                        platformType);
            }
        }
        return new NtpBackgroundDataGroup();
    }

    /**
     * Returns the saved NTP's background sync data of the given platform type as a {@link
     * JSONArray}.
     *
     * @param platformType The platform type to get the background data for.
     * @return The background data for the given platform type.
     */
    @Nullable JSONArray getJsonArrayFromSharedPreferenceImpl(@PlatformType int platformType) {
        String historyData =
                ChromeSharedPreferences.getInstance()
                        .readString(getSharedPreferenceKey(platformType), null);
        if (historyData == null || historyData.isEmpty()) return null;
        try {
            return new JSONArray(historyData);
        } catch (JSONException e) {
            Log.i(
                    TAG,
                    "Failed to convert NTP's sync background data to JSONArray: platform"
                            + " type = %d.",
                    platformType);
            return null;
        }
    }

    /**
     * Writes the given {@link JSONArray} to shared preferences for the specified platform type.
     *
     * @param current The JSON array to write.
     * @param platformType The platform type associated with the data.
     */
    private void writeToSharedPreference(JSONArray current, @PlatformType int platformType) {
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
