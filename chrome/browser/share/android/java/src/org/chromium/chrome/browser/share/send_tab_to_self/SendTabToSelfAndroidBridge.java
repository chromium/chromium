// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.WebContents;

import java.util.List;

/**
 * Bridge to interface with send_tab_to_self_android_bridge which interacts with the corresponding
 * sync service. This is used by SendTabToSelfShareActivity when a user taps to share a tab. The
 * bridge is created and destroyed within the same method call.
 */
@JNINamespace("send_tab_to_self")
@NullMarked
public class SendTabToSelfAndroidBridge {
    /** Interface for a callback to receive the result of a send tab to self operation. */
    @FunctionalInterface
    public interface CommitConfirmationCallback {
        @CalledByNative("CommitConfirmationCallback")
        void onResult(@SendTabToSelfResult int result);
    }

    // TODO(crbug.com/40618597): Add logic back in to track whether model is loaded.
    // private boolean mIsNativeSendTabToSelfModelLoaded;

    /**
     * Handles the action when the user selects a device.
     *
     * @param webContents The web contents that the user is sharing.
     * @param targetDeviceSyncCacheGuid The GUID of the target device.
     * @param url The URL being shared.
     * @param title The title of the page being shared.
     */
    public static void sendTabToDevice(
            @Nullable WebContents webContents,
            String targetDeviceSyncCacheGuid,
            String url,
            String title,
            CommitConfirmationCallback commitConfirmation) {
        SendTabToSelfAndroidBridgeJni.get()
                .sendTabToDevice(
                        webContents, targetDeviceSyncCacheGuid, url, title, commitConfirmation);
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
     * Marks the entry associated with the GUID as opened.
     *
     * @param profile Profile of the user to mark entry for.
     * @param guid The GUID to mark the entry for.
     */
    public static void markEntryOpened(Profile profile, String guid) {
        SendTabToSelfAndroidBridgeJni.get().markEntryOpened(profile, guid);
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

    public static @Nullable @EntryPointDisplayReason Integer getEntryPointDisplayReason(
            Profile profile, String url) {
        return SendTabToSelfAndroidBridgeJni.get().getEntryPointDisplayReason(profile, url);
    }

    @NativeMethods
    public interface Natives {
        void sendTabToDevice(
                @Nullable WebContents webContents,
                String targetDeviceSyncCacheGuid,
                String url,
                String title,
                CommitConfirmationCallback commitConfirmation);

        void deleteEntry(@JniType("Profile*") Profile profile, String guid);

        void markEntryOpened(@JniType("Profile*") Profile profile, String guid);

        void dismissEntry(@JniType("Profile*") Profile profile, String guid);

        @JniType("std::vector")
        List<TargetDeviceInfo> getAllTargetDeviceInfos(@JniType("Profile*") Profile profile);

        void updateActiveWebContents(WebContents webContents);

        @Nullable Integer getEntryPointDisplayReason(
                @JniType("Profile*") Profile profile, String url);
    }
}
