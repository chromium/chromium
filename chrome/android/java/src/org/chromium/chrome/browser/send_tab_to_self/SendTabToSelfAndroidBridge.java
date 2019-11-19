// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.send_tab_to_self;

import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.List;

/**
 * Bridge to interface with send_tab_to_self_android_bridge which interacts with the corresponding
 * sync service. This is used by SendTabToSelfShareActivity when a user taps to share a tab. The
 * bridge is created and destroyed within the same method call.
 */
@JNINamespace("send_tab_to_self")
public class SendTabToSelfAndroidBridge {
    // TODO(https://crbug.com/942549): Add logic back in to track whether model is loaded.
    private boolean mIsNativeSendTabToSelfModelLoaded;

    /**
     * @param profile Profile of the user to retrieve the GUIDs for.
     * @returns All GUIDs for all SendTabToSelf entries
     */
    public static List<String> getAllGuids(Profile profile) {
        // TODO(https://crbug.com/942549): Add this assertion back in once the code to load is in
        // place. assert mIsNativeSendTabToSelfModelLoaded;
        List<String> toPopulate = new ArrayList<String>();
        SendTabToSelfAndroidBridgeJni.get().getAllGuids(profile, toPopulate);
        return toPopulate;
    }

    /**
     * Called by the native code in order to populate the list.
     *
     * @param allGuids List to populate provided by getAllGuids
     * @param newGuid The GUID to add to the list
     */
    @CalledByNative
    private static void addToGuidList(List<String> allGuids, String newGuid) {
        allGuids.add(newGuid);
    }

    /**
     * Deletes all SendTabToSelf entries. This is called when the user disables sync.
     */
    public static void deleteAllEntries(Profile profile) {
        // TODO(https://crbug.com/942549): Add this assertion back in once the code to load is in
        // place. assert mIsNativeSendTabToSelfModelLoaded;
        SendTabToSelfAndroidBridgeJni.get().deleteAllEntries(profile);
    }

    /**
     * Creates a new entry to be persisted to the sync backend.
     *
     * @param profile Profile of the user to add entry for.
     * @param url URL to be shared
     * @param title Title of the page
     * @param navigationTime Time the user navigated to the page
     * @return If the persistent entry in the bridge was created.
     */
    public static SendTabToSelfEntry addEntry(Profile profile, String url, String title,
            long navigationTime, String targetDeviceSyncCacheGuid) {
        // TODO(https://crbug.com/942549): Add this assertion back in once the code to load is in
        // place. assert mIsNativeSendTabToSelfModelLoaded;
        return SendTabToSelfAndroidBridgeJni.get().addEntry(
                profile, url, title, navigationTime, targetDeviceSyncCacheGuid);
    }

    /**
     * Return the entry associated with a particular GUID
     *
     * @param profile Profile of the user to get entry for.
     * @param guid The GUID to retrieve the entry for
     * @return The found entry or null if none exists
     */
    @Nullable
    public static SendTabToSelfEntry getEntryByGUID(Profile profile, String guid) {
        // TODO(https://crbug.com/942549): Add this assertion back in once the code to load is in
        // place. assert mIsNativeSendTabToSelfModelLoaded;
        return SendTabToSelfAndroidBridgeJni.get().getEntryByGUID(profile, guid);
    }

    /**
     * Deletes the entry associated with the GUID.
     *
     * @param profile Profile of the user to delete entry for.
     * @param guid The GUID to delete the entry for.
     */
    public static void deleteEntry(Profile profile, String guid) {
        SendTabToSelfAndroidBridgeJni.get().deleteEntry(profile, guid);
    }

    /**
     * Dismiss the entry associated with the GUID.
     *
     * @param profile Profile of the user to dismiss entry for.
     * @param guid The GUID to dismiss the entry for.
     */
    public static void dismissEntry(Profile profile, String guid) {
        SendTabToSelfAndroidBridgeJni.get().dismissEntry(profile, guid);
    }
    /**
     * Mark the entry associated with the GUID as opened.
     *
     * @param profile Profile of the user to mark entry as opened.
     * @param guid The GUID of the entry to mark as opened.
     */
    public static void markEntryOpened(Profile profile, String guid) {
        SendTabToSelfAndroidBridgeJni.get().markEntryOpened(profile, guid);
    }

    /**
     * Return whether the feature is available for the current user Profile and
     * the current WebContents.
     *
     * @param WebContents The current WebContents.
     * @return Whether the feature is available.
     */
    public static boolean isFeatureAvailable(WebContents webContents) {
        return SendTabToSelfAndroidBridgeJni.get().isFeatureAvailable(webContents);
    }

    /**
     * Shows an infobar for the webcontents passed in.
     *
     * @param entry Contains the URL to open when the user taps on the infobar.
     * @param webContents Where to create the infobar.
     */
    public static void showInfoBar(SendTabToSelfEntry entry, WebContents webContents) {
        SendTabToSelfAndroidBridgeJni.get().showInfoBar(
                webContents, entry.guid, entry.url, entry.targetDeviceSyncCacheGuid);
    }

    /**
     * @param profile Profile of the user for whom to retrieve the targetDeviceInfos.
     * @returns All {@link TargetDeviceInfo} for the user.
     */
    public static List<TargetDeviceInfo> getAllTargetDeviceInfos(Profile profile) {
        // TODO(https://crbug.com/942549): Add this assertion back in once the
        // code to load is in place. assert mIsNativeSendTabToSelfModelLoaded;
        List<TargetDeviceInfo> toPopulate = new ArrayList<TargetDeviceInfo>();
        SendTabToSelfAndroidBridgeJni.get().getAllTargetDeviceInfos(profile, toPopulate);
        return toPopulate;
    }

    /**
     * Called by the native code in order to populate the list.
     *
     * @param allInfos List to populate provided by getAllTargetDeviceInfos.
     * @param newInfo The DeviceInfo to add to the list.
     */
    @CalledByNative
    private static void addToTargetDeviceInfoList(
            List<TargetDeviceInfo> allInfos, TargetDeviceInfo newInfo) {
        allInfos.add(newInfo);
    }

    @NativeMethods
    interface Natives {
        SendTabToSelfEntry addEntry(Profile profile, String url, String title, long navigationTime,
                String targetDeviceSyncCacheGuid);

        void getAllGuids(Profile profile, List<String> guids);

        void deleteAllEntries(Profile profile);

        void deleteEntry(Profile profile, String guid);

        void dismissEntry(Profile profile, String guid);

        void markEntryOpened(Profile profile, String guid);

        SendTabToSelfEntry getEntryByGUID(Profile profile, String guid);

        boolean isFeatureAvailable(WebContents webContents);

        void showInfoBar(
                WebContents webContents, String guid, String url, String targetDeviceSyncCacheGuid);

        void getAllTargetDeviceInfos(Profile profile, List<TargetDeviceInfo> guids);
    }
}
