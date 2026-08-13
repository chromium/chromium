// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.synced_set_up;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.sync_preferences.cross_device_pref_tracker.CrossDevicePrefTracker;
import org.chromium.components.sync_preferences.synced_set_up.PrefToValueMapBridge;

import java.util.Map;

/** Allows access to components/sync_preferences/synced_set_up/utils.cc. */
@NullMarked
@JNINamespace("sync_preferences::synced_set_up")
public class SyncedSetUpUtilsBridge {

    private static @Nullable Map<String, Object> sCrossDeviceSettingsForTesting;

    /**
     * Retrieves the cross-device preferences from a remote device.
     *
     * @param prefTracker The {@link CrossDevicePrefTracker} to use.
     * @param profile The {@link Profile} to use.
     * @return A map of preference names to their values.
     */
    public static Map<String, Object> getCrossDevicePrefsFromRemoteDevice(
            CrossDevicePrefTracker prefTracker, Profile profile) {
        if (sCrossDeviceSettingsForTesting != null) return sCrossDeviceSettingsForTesting;

        long prefTrackerPtr = prefTracker.getNativePtr();
        if (prefTrackerPtr == 0) return Map.of();

        PrefToValueMapBridge mapBridge = new PrefToValueMapBridge();
        SyncedSetUpUtilsBridgeJni.get()
                .getCrossDevicePrefsFromRemoteDevice(
                        profile.getNativeBrowserContextPointer(),
                        prefTrackerPtr,
                        mapBridge.getNativeBridgePtr());
        Map<String, Object> result = mapBridge.getPrefValueMap();
        mapBridge.destroy();
        return result;
    }

    /**
     * For testing: {@code null} indicates that no mock value is set for testing (production logic
     * should run). An empty string ({@code ""}) should be set to represent the case where no
     * matching device GUID is found.
     */
    private static @Nullable String sBestMatchDeviceGuidForTesting;

    /**
     * Returns the best match remote device GUID for synced set up, or null if none found.
     *
     * <p>Native C++ returns an empty string ({@code ""}) when no matching device GUID exists or
     * when dependencies are unavailable; this method translates that empty string into {@code
     * null}.
     *
     * @param prefTracker The {@link CrossDevicePrefTracker} to use.
     * @param profile The {@link Profile} to use.
     * @return The best match device GUID, or null if no matching device exists.
     */
    public static @Nullable String getBestMatchDeviceGuid(
            CrossDevicePrefTracker prefTracker, Profile profile) {
        if (sBestMatchDeviceGuidForTesting != null) {
            return sBestMatchDeviceGuidForTesting.isEmpty() ? null : sBestMatchDeviceGuidForTesting;
        }

        long prefTrackerPtr = prefTracker.getNativePtr();
        if (prefTrackerPtr == 0) return null;

        String guid =
                SyncedSetUpUtilsBridgeJni.get()
                        .getBestMatchDeviceGuid(
                                profile.getNativeBrowserContextPointer(), prefTrackerPtr);
        return (guid == null || guid.isEmpty()) ? null : guid;
    }

    /**
     * Sets the best match device GUID for testing. Pass {@code null} to reset and use production
     * logic, or pass an empty string ({@code ""}) to simulate the case where no matching device
     * GUID exists.
     *
     * @param guid The test device GUID, {@code ""} for no matching device, or {@code null} to reset
     *     to production code.
     */
    public static void setBestMatchDeviceGuidForTesting(@Nullable String guid) {
        @Nullable String oldState = sBestMatchDeviceGuidForTesting;
        sBestMatchDeviceGuidForTesting = guid;
        ResettersForTesting.register(
                () -> {
                    sBestMatchDeviceGuidForTesting = oldState;
                });
    }

    public static void setCrossDeviceSettingsForTesting(@Nullable Map<String, Object> map) {
        @Nullable Map<String, Object> oldState = sCrossDeviceSettingsForTesting;
        sCrossDeviceSettingsForTesting = map;
        ResettersForTesting.register(
                () -> {
                    sCrossDeviceSettingsForTesting = oldState;
                });
    }

    @NativeMethods
    public interface Natives {
        void getCrossDevicePrefsFromRemoteDevice(
                long profile, long crossDevicePrefTracker, long mapBridge);

        @JniType("std::string")
        String getBestMatchDeviceGuid(long profile, long crossDevicePrefTracker);
    }
}
