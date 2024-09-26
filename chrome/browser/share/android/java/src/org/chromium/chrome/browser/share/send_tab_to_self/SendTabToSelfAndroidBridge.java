// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import androidx.annotation.Nullable;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.WebContents;

import java.util.List;
import java.util.Optional;

/**
 * Bridge to interface with send_tab_to_self_android_bridge which interacts with the corresponding
 * sync service. This is used by SendTabToSelfShareActivity when a user taps to share a tab. The
 * bridge is created and destroyed within the same method call.
 */
@JNINamespace("send_tab_to_self")
public class SendTabToSelfAndroidBridge {
    // TODO(crbug.com/40618597): Add logic back in to track whether model is loaded.
    private boolean mIsNativeSendTabToSelfModelLoaded;

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
        // TODO(crbug.com/40618597): Add this assertion back in once the code to load is in
        // place. assert mIsNativeSendTabToSelfModelLoaded;
        return SendTabToSelfAndroidBridgeJni.get()
                .addEntry(profile, url, title, targetDeviceSyncCacheGuid);
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
     * @param profile Profile of the user for whom to retrieve the targetDeviceInfos.
     * @return All {@link TargetDeviceInfo} for the user, or an empty list if the model isn't ready.
     */
    public static List<TargetDeviceInfo> getAllTargetDeviceInfos(Profile profile) {
        // TODO(crbug.com/40618597): Add this assertion back in once the
        // code to load is in place.
        // assert mIsNativeSendTabToSelfModelLoaded;
        return SendTabToSelfAndroidBridgeJni.get().getAllTargetDeviceInfos(profile);
    }

    /**
     * @param webContents WebContents where a navigation was just completed.
     */
    public static void updateActiveWebContents(WebContents webContents) {
        SendTabToSelfAndroidBridgeJni.get().updateActiveWebContents(webContents);
    }

    public static Optional</*@EntryPointDisplayReason*/ Integer> getEntryPointDisplayReason(
            Profile profile, String url) {
        @Nullable
        Integer reason =
                SendTabToSelfAndroidBridgeJni.get().getEntryPointDisplayReason(profile, url);
        return reason == null ? Optional.empty() : Optional.of(reason.intValue());
    }

    @NativeMethods
    public interface Natives {
        boolean addEntry(
                @JniType("Profile*") Profile profile,
                String url,
                String title,
                String targetDeviceSyncCacheGuid);

        void deleteEntry(@JniType("Profile*") Profile profile, String guid);

        void dismissEntry(@JniType("Profile*") Profile profile, String guid);

        @JniType("std::vector")
        List<TargetDeviceInfo> getAllTargetDeviceInfos(@JniType("Profile*") Profile profile);

        void updateActiveWebContents(WebContents webContents);

        @Nullable
        Integer getEntryPointDisplayReason(@JniType("Profile*") Profile profile, String url);
    }
}
