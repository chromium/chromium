// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.WebContents;

import java.util.Arrays;
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
     * @return All GUIDs for all SendTabToSelf entries, or an empty list if the model isn't ready.
     */
    public static List<String> getAllGuids(Profile profile) {
        // TODO(https://crbug.com/942549): Add this assertion back in once the code to load is in
        // place. assert mIsNativeSendTabToSelfModelLoaded;
        return Arrays.asList(SendTabToSelfAndroidBridgeJni.get().getAllGuids(profile));
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
     * @return If the persistent entry in the bridge was created.
     */
    public static boolean addEntry(
            Profile profile, String url, String title, String targetDeviceSyncCacheGuid) {
        // TODO(https://crbug.com/942549): Add this assertion back in once the code to load is in
        // place. assert mIsNativeSendTabToSelfModelLoaded;
        return SendTabToSelfAndroidBridgeJni.get().addEntry(
                profile, url, title, targetDeviceSyncCacheGuid);
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
     * @param profile Profile of the user for whom to retrieve the targetDeviceInfos.
     * @return All {@link TargetDeviceInfo} for the user, or an empty list if the model isn't ready.
     */
    public static List<TargetDeviceInfo> getAllTargetDeviceInfos(Profile profile) {
        // TODO(https://crbug.com/942549): Add this assertion back in once the
        // code to load is in place. assert mIsNativeSendTabToSelfModelLoaded;
        return Arrays.asList(SendTabToSelfAndroidBridgeJni.get().getAllTargetDeviceInfos(profile));
    }

    /**
     * @param webContents WebContents where a navigation was just completed.
     * @param profile Profile to which |webContents| belongs.
     */
    public static void updateActiveWebContents(WebContents webContents) {
        SendTabToSelfAndroidBridgeJni.get().updateActiveWebContents(webContents);
    }

    @NativeMethods
    public interface Natives {
        boolean addEntry(
                Profile profile, String url, String title, String targetDeviceSyncCacheGuid);

        String[] getAllGuids(Profile profile);

        void deleteAllEntries(Profile profile);

        void deleteEntry(Profile profile, String guid);

        void dismissEntry(Profile profile, String guid);

        void markEntryOpened(Profile profile, String guid);

        TargetDeviceInfo[] getAllTargetDeviceInfos(Profile profile);

        void updateActiveWebContents(WebContents webContents);
    }
}
