// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_sync.data;

import android.content.Context;

import androidx.annotation.ColorInt;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

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
                if (existingData instanceof NtpBackgroundDataImageBase existingImage
                        && backgroundData instanceof NtpBackgroundDataImageBase newData) {
                    @Nullable
                    @ColorInt
                    Integer resolvedColor =
                            newData.getPrimaryColor() != null
                                    ? newData.getPrimaryColor()
                                    : existingImage.getPrimaryColor();
                    // If existing entry has enriched metadata (e.g., BackgroundImageInfo fetched
                    // later), preserve the enriched existing entry instead of overwriting with
                    // incomplete native data.
                    if (existingImage.getBackgroundImageInfo() != null
                            && newData.getBackgroundImageInfo() == null) {
                        dataToSave = existingData;
                    }
                    ((NtpBackgroundDataImageBase) dataToSave).setPrimaryColor(resolvedColor);
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
                () ->
                        replaceBackgroundDataInPlace(
                                themeCollectionData, themeCollectionData.getPlatformType()));
    }

    /**
     * Replaces the entry of the {@code groupPlatformTypeToUpdate} list that is the same theme as
     * {@code backgroundData} (see {@link NtpBackgroundDataBase#equals}) with {@code
     * backgroundData}, at the same index and with the platform type of the replaced entry. Nothing
     * is written if there is no such entry.
     *
     * @param backgroundData The background data to store in place of the existing entry.
     * @param groupPlatformTypeToUpdate The platform type of the group list to update. It may differ
     *     from the platform type of {@code backgroundData}, e.g. the local list is updated with a
     *     rotated background whatever platform it carries.
     */
    private void replaceBackgroundDataInPlace(
            NtpBackgroundDataBase backgroundData, @PlatformType int groupPlatformTypeToUpdate) {
        try {
            NtpBackgroundDataGroup currentGroup =
                    getBackgroundDataGroupFromSharedPreference(groupPlatformTypeToUpdate);
            if (currentGroup.isEmpty()) return;

            int index = currentGroup.indexOf(backgroundData);
            if (index == -1) return;

            // The entry keeps the platform it was selected from, which equals() doesn't compare:
            // an entry of the local list selected from the history of another device carries that
            // device's platform type, and must keep counting as the latest selection from it.
            JSONObject json = backgroundData.toJson();
            json.put(
                    NtpBackgroundDataBase.PLATFORM_TYPE_KEY,
                    currentGroup.get(index).getPlatformType());
            JSONArray jsonArray = currentGroup.toJsonArray();
            jsonArray.put(index, json);
            writeToSharedPreference(jsonArray, groupPlatformTypeToUpdate);
        } catch (JSONException e) {
            Log.i(
                    TAG,
                    "Failed to replace NTP's sync background data in the SharedPreference: platform"
                            + " type = %d, data type = %d.",
                    groupPlatformTypeToUpdate,
                    backgroundData.getBackgroundType());
        }
    }

    /**
     * Replaces the entry of the local history list that is the same theme as {@code backgroundData}
     * with {@code backgroundData}, at the same index. Used when a theme with daily refresh rotates
     * on this device, so that its entry shows the rotated color or image like the current
     * background.
     *
     * @param backgroundData The rotated background. Its platform type is ignored.
     */
    public void maybeReplaceUserSelectedBackgroundInPlace(NtpBackgroundDataBase backgroundData) {
        replaceBackgroundDataInPlace(backgroundData, PlatformType.ANDROID);
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
     * Removes the image file for the given synced {@link NtpBackgroundDataImageBase} if it is no
     * longer referenced in any local or remote history list.
     *
     * @param imageBaseData The synced image base data to clean up.
     */
    public void maybeCleanUpUnusedSyncedImageData(NtpBackgroundDataImageBase imageBaseData) {
        PostTask.postTask(
                TaskTraits.USER_VISIBLE_MAY_BLOCK,
                () -> maybeDeleteImageFileIfNotInUse(imageBaseData, /* platformToSkip= */ null));
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
        int platformToSkip;
        if (isLocalSelected) {
            // If the data comes from the local history list, checks if the fileIdHash exists in any
            // remote groups.
            platformToSkip = PlatformType.ANDROID;
        } else {
            // If the data comes from a remote platform list, checks local group and other remote
            // groups which are different from the data's platform type.
            platformToSkip = imageBaseData.getPlatformType();
            assert platformToSkip != PlatformType.ANDROID;
        }
        maybeDeleteImageFileIfNotInUse(imageBaseData, platformToSkip);
    }

    /**
     * Deletes the image file for the given {@link NtpBackgroundDataImageBase} if its file ID hash
     * is not referenced in any checked history lists in SharedPreferences.
     *
     * @param imageBaseData The image base data to clean up.
     * @param platformToSkip Optional platform type to skip checking, or null to check all
     *     platforms.
     */
    private void maybeDeleteImageFileIfNotInUse(
            NtpBackgroundDataImageBase imageBaseData, @Nullable Integer platformToSkip) {
        String fileIdHash = imageBaseData.getFileIdHash();
        if (fileIdHash == null) return;

        for (int i = PlatformType.ANDROID; i < PlatformType.MAX_COUNT; i++) {
            if (platformToSkip != null && i == platformToSkip.intValue()) continue;

            NtpBackgroundDataGroup group = getBackgroundDataGroupFromSharedPreference(i);
            if (isImageStillInUse(group, fileIdHash)) {
                return;
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

    /** Removes all background data from non-Android platforms from shared preferences. */
    public void removeAllNonAndroidPlatformData() {
        PostTask.postTask(
                TaskTraits.USER_VISIBLE_MAY_BLOCK, this::removeAllNonAndroidPlatformDataImpl);
    }

    /**
     * Removes all background data from non-Android platforms and deletes related image files if
     * they aren't used by local history.
     */
    private void removeAllNonAndroidPlatformDataImpl() {
        SharedPreferencesManager sharedPreferencesManager = ChromeSharedPreferences.getInstance();
        List<NtpBackgroundDataImageBase> imageDataToCleanUp = new ArrayList<>();

        for (int i = PlatformType.ANDROID + 1; i < PlatformType.MAX_COUNT; i++) {
            NtpBackgroundDataGroup group = getBackgroundDataGroupFromSharedPreference(i);
            for (NtpBackgroundDataBase data : group) {
                if (data instanceof NtpBackgroundDataImageBase imageBaseData) {
                    imageDataToCleanUp.add(imageBaseData);
                }
            }
            sharedPreferencesManager.removeKey(getSharedPreferenceKey(i));
        }

        // When removing a remote sync data, we will delete its image file as long as it isn't used
        // by the local selected group, i.e., Platform Android.
        NtpBackgroundDataGroup groupToCheck =
                getBackgroundDataGroupFromSharedPreference(PlatformType.ANDROID);
        for (NtpBackgroundDataImageBase imageBaseData : imageDataToCleanUp) {
            maybeDeleteImageFileIfNotInUseInGroup(imageBaseData, groupToCheck);
        }
    }

    /**
     * Deletes the image file if it isn't used in the provided group.
     *
     * @param imageBaseData The background image data.
     * @param groupToCheck The group of background data to check.
     */
    private void maybeDeleteImageFileIfNotInUseInGroup(
            NtpBackgroundDataImageBase imageBaseData, NtpBackgroundDataGroup groupToCheck) {
        String fileIdHash = imageBaseData.getFileIdHash();
        if (fileIdHash == null) return;

        if (isImageStillInUse(groupToCheck, fileIdHash)) {
            return;
        }

        NtpCustomizationUtils.maybeDeleteFile(
                NtpCustomizationUtils.getBackgroundImageFileFromPath(
                        imageBaseData.getLastUploadImageFilePath()));
    }

    /** Resets the shared preferences used by this manager. */
    public static void resetSharedPreference() {
        ChromeSharedPreferences.getInstance()
                .removeKeysWithPrefix(ChromePreferenceKeys.NTP_CUSTOMIZATION_SYNC_HISTORY_DATA);
    }
}
