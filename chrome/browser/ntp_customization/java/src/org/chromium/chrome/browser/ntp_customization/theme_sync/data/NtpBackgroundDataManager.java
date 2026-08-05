// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_sync.data;

import android.content.Context;

import org.json.JSONArray;
import org.json.JSONException;

import org.chromium.base.Log;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

import java.util.ArrayList;
import java.util.List;

/** Centralizes management of NTP background preference data. */
@NullMarked
public class NtpBackgroundDataManager {
    public static final int MAXIMUM_LOCAL_HISTORY = 3;

    private static final String TAG = "NtpBackgroundData";
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
     *
     * @param backgroundDataGroup The group of background data to save.
     */
    public void saveRemoteSyncDataToSharedPreference(NtpBackgroundDataGroup backgroundDataGroup) {
        for (NtpBackgroundDataBase data : backgroundDataGroup) {
            if (data.getPlatformType() <= PlatformType.ANDROID) continue;
            saveRemoteSyncDataToSharedPreference(data);
        }
    }

    /**
     * Saves a single NTP's background type from cross device sync to the shared preference.
     *
     * @param backgroundData The background data to save.
     */
    public void saveRemoteSyncDataToSharedPreference(NtpBackgroundDataBase backgroundData) {
        PostTask.postTask(
                TaskTraits.USER_VISIBLE_MAY_BLOCK,
                () -> saveRemoteSyncDataToSharedPreferenceImpl(backgroundData));
    }

    private void saveRemoteSyncDataToSharedPreferenceImpl(NtpBackgroundDataBase backgroundData) {
        try {
            @PlatformType int platformType = backgroundData.getPlatformType();
            NtpBackgroundDataGroup currentGroup =
                    getBackgroundDataGroupFromSharedPreference(platformType);

            if (currentGroup.isEmpty()) {
                currentGroup.add(backgroundData);
                writeToSharedPreference(currentGroup.toJsonArray(), platformType);
                return;
            }

            // To update existing remote sync data:
            // If this backgroundData already in the current remote sync data list, moves it to the
            // first one. Otherwise, adds it as the first one on the list and removed the last data
            // of the list if exceeds the maximum allowed size of history data.
            int index = currentGroup.indexOf(backgroundData);
            NtpBackgroundDataBase dataToSave = backgroundData;
            if (index != -1) {
                NtpBackgroundDataBase existingData = currentGroup.remove(index);
                // If existing entry has enriched metadata (e.g., BackgroundImageInfo fetched
                // later), preserve the enriched existing entry instead of overwriting with
                // incomplete native data.
                if (existingData instanceof NtpBackgroundDataImageBase existingImage
                        && existingImage.getBackgroundImageInfo() != null
                        && backgroundData instanceof NtpBackgroundDataImageBase newData
                        && newData.getBackgroundImageInfo() == null) {
                    dataToSave = existingData;
                }
            } else {
                if (currentGroup.size() >= MAXIMUM_REMOTE_HISTORY) {
                    NtpBackgroundDataBase dataToRemove = currentGroup.get(currentGroup.size() - 1);
                    currentGroup.remove(currentGroup.size() - 1);
                    if (dataToRemove instanceof NtpBackgroundDataImageBase imageBaseData) {
                        cleanUpForBackgroundData(imageBaseData, /* isLocalSelected= */ false);
                    }
                }
            }
            currentGroup.add(0, dataToSave);

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
     * Saves a single NTP's background type from cross device sync to the shared preference.
     *
     * @param themeCollectionData The background data to save.
     */
    public void updateRemoteSyncDataToSharedPreference(
            NtpBackgroundDataThemeCollection themeCollectionData) {
        PostTask.postTask(
                TaskTraits.USER_VISIBLE_MAY_BLOCK,
                () -> updateRemoteSyncDataToSharedPreferenceImpl(themeCollectionData));
    }

    private void updateRemoteSyncDataToSharedPreferenceImpl(
            NtpBackgroundDataThemeCollection themeCollectionToUpdate) {
        try {
            @PlatformType int platformType = themeCollectionToUpdate.getPlatformType();
            NtpBackgroundDataGroup currentGroup =
                    getBackgroundDataGroupFromSharedPreference(platformType);
            if (currentGroup.isEmpty()) return;

            int index = currentGroup.indexOf(themeCollectionToUpdate);
            if (index == -1) return;

            currentGroup.getList().set(index, themeCollectionToUpdate);
            // Updates existing remote sync data.
            writeToSharedPreference(
                    currentGroup.toJsonArray(), themeCollectionToUpdate.getPlatformType());
        } catch (JSONException e) {
            Log.i(
                    TAG,
                    "Failed to save NTP's sync background data to the SharedPreference: platform"
                            + " type = %d, data type = %d.",
                    themeCollectionToUpdate.getPlatformType(),
                    themeCollectionToUpdate.getBackgroundType());
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
            @PlatformType int platformTypeToSave = PlatformType.ANDROID;
            NtpBackgroundDataGroup currentGroup =
                    getBackgroundDataGroupFromSharedPreference(platformTypeToSave);

            if (currentGroup.isEmpty()) {
                currentGroup.add(backgroundData);
                writeToSharedPreference(currentGroup.toJsonArray(), platformTypeToSave);
                return;
            }

            // To update user selected background history data:
            // If the user chose a cross device synced background type, we add the type to the local
            // selection history list, but remove any existing type from that platform from the
            // local selection history. This allows to cache only the latest chosen background type
            // from any remote platform.
            List<NtpBackgroundDataBase> removedItems = new ArrayList<>();
            int platformTypeOfNewData = backgroundData.getPlatformType();
            if (platformTypeOfNewData != PlatformType.ANDROID) {
                currentGroup
                        .getList()
                        .removeIf(
                                item -> {
                                    if (item.getPlatformType() == platformTypeOfNewData) {
                                        removedItems.add(item);
                                        return true;
                                    }
                                    return false;
                                });
            }

            // If the backgroundData already in local history, removes the existing one.
            int index = currentGroup.indexOf(backgroundData);
            if (index != -1) {
                currentGroup.remove(index);
            }
            currentGroup.add(0, backgroundData);
            if (currentGroup.size() > MAXIMUM_LOCAL_HISTORY) {
                int indexToRemove = currentGroup.size() - 1;
                removedItems.add(currentGroup.get(indexToRemove));
                currentGroup.remove(indexToRemove);
            }
            writeToSharedPreference(currentGroup.toJsonArray(), platformTypeToSave);

            // Cleans up all removed items. Because we just wrote the new list to shared preference,
            // isImageStillInUse() will correctly see that backgroundData is in the list,
            // and will not prematurely delete its file.
            for (NtpBackgroundDataBase removedItem : removedItems) {
                if (removedItem instanceof NtpBackgroundDataImageBase imageBaseData) {
                    cleanUpForBackgroundData(imageBaseData, /* isLocalSelected= */ true);
                }
            }
        } catch (JSONException e) {
            Log.i(
                    TAG,
                    "Failed to save user selected NTP's sync background data to the"
                            + " SharedPreference: data type = %d.",
                    backgroundData.getBackgroundType());
        }
    }

    /**
     * Removes the image file for the backgroundData if it is no longer referenced in any other
     * local or remote history list.
     *
     * @param imageBaseData The image base data to clean up.
     * @param isLocalSelected Whether the cleanup was triggered by an eviction from the local
     *     history list.
     */
    private void cleanUpForBackgroundData(
            NtpBackgroundDataImageBase imageBaseData, boolean isLocalSelected) {
        String fileIdHash = imageBaseData.getFileIdHash();
        if (fileIdHash == null) return;

        if (isLocalSelected) {
            // If the data comes from the local history list, checks if the fileIdHash exists in
            // any remote groups.
            for (int i = PlatformType.ANDROID + 1; i < PlatformType.MAX_COUNT; i++) {
                NtpBackgroundDataGroup remoteGroup = getBackgroundDataGroupFromSharedPreference(i);
                if (isImageStillInUse(remoteGroup, fileIdHash)) return;
            }
        } else {
            // If the data comes from a remote platform list, checks local group and other remote
            // groups which are different from the data's platform type.
            int ownRemotePlatform = imageBaseData.getPlatformType();
            assert ownRemotePlatform != PlatformType.ANDROID;

            for (int i = PlatformType.ANDROID; i < PlatformType.MAX_COUNT; i++) {
                if (i == ownRemotePlatform) continue;

                NtpBackgroundDataGroup group = getBackgroundDataGroupFromSharedPreference(i);
                if (isImageStillInUse(group, fileIdHash)) return;
            }
        }

        NtpCustomizationUtils.maybeDeleteFile(
                NtpCustomizationUtils.getBackgroundImageFileFromPath(
                        imageBaseData.getLastUploadImageFilePath()));
    }

    /**
     * Checks if any theme collection or upload image in the given group matches the specified file
     * ID hash.
     *
     * @param group The history data group to search in.
     * @param fileIdHash The unique file ID hash of the image to look for.
     * @return True if a matching image is found in the group, false otherwise.
     */
    private boolean isImageStillInUse(NtpBackgroundDataGroup group, String fileIdHash) {
        for (NtpBackgroundDataBase data : group) {
            if (data instanceof NtpBackgroundDataImageBase otherImageBaseData) {
                if (fileIdHash.equals(otherImageBaseData.getFileIdHash())) {
                    return true;
                }
            }
        }
        return false;
    }

    /**
     * Returns the saved NTP's background history data of the given platform type as a list of
     * {@link NtpBackgroundDataBase}.
     *
     * @return The background data for the given platform type.
     */
    public NtpBackgroundDataGroup[] getBackgroundDataListFromSharedPreference() {
        NtpBackgroundDataGroup[] dataList = new NtpBackgroundDataGroup[PlatformType.MAX_COUNT];
        for (int i = PlatformType.ANDROID; i < PlatformType.MAX_COUNT; i++) {
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
        for (int i = PlatformType.ANDROID; i < PlatformType.MAX_COUNT; i++) {
            sharedPreferencesManager.removeKey(getSharedPreferenceKey(i));
        }
    }
}
