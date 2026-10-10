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
import org.chromium.components.sync_device_info.FormFactor;
import org.chromium.components.sync_device_info.OsType;
import org.chromium.components.sync_preferences.cross_device_pref_tracker.CrossDevicePrefTracker;
import org.chromium.components.sync_preferences.synced_set_up.PrefToValueMapBridge;

import java.util.Map;

/** Allows access to components/sync_preferences/synced_set_up/utils.cc. */
@NullMarked
@JNINamespace("sync_preferences::synced_set_up")
public class SyncedSetUpUtilsBridge {

    /** Holds the OS type and form factor of a remote device. */
    public static class DeviceOsAndFormFactor {
        public final @OsType int osType;
        public final @FormFactor int formFactor;

        public DeviceOsAndFormFactor(@OsType int osType, @FormFactor int formFactor) {
            this.osType = osType;
            this.formFactor = formFactor;
        }
    }

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
     * should run). {@link OsType#UNKNOWN} in {@code osType} represents the case where no matching
     * device is found.
     */
    private static @Nullable DeviceOsAndFormFactor sBestMatchDeviceForTesting;

    /**
     * Returns the best match remote device OS type and form factor for synced set up, or null if
     * none found.
     *
     * <p>Native C++ returns {@link OsType#UNKNOWN} when no matching device exists or when
     * dependencies are unavailable; this method translates {@link OsType#UNKNOWN} into {@code
     * null}.
     *
     * @param prefTracker The {@link CrossDevicePrefTracker} to use.
     * @param profile The {@link Profile} to use.
     * @return The best match device {@link DeviceOsAndFormFactor}, or null if no matching device
     *     exists.
     */
    public static @Nullable DeviceOsAndFormFactor getBestMatchDeviceOsTypeAndFormFactor(
            CrossDevicePrefTracker prefTracker, Profile profile) {
        if (sBestMatchDeviceForTesting != null) {
            return sBestMatchDeviceForTesting.osType == OsType.UNKNOWN
                    ? null
                    : sBestMatchDeviceForTesting;
        }

        long prefTrackerPtr = prefTracker.getNativePtr();
        if (prefTrackerPtr == 0) return null;

        int[] result =
                SyncedSetUpUtilsBridgeJni.get()
                        .getBestMatchDeviceOsTypeAndFormFactor(
                                profile.getNativeBrowserContextPointer(), prefTrackerPtr);
        if (result == null || result.length < 2 || result[0] == OsType.UNKNOWN) {
            return null;
        }
        return new DeviceOsAndFormFactor(result[0], result[1]);
    }

    /**
     * Sets the best match device OS type and form factor for testing. Pass {@code null} for {@code
     * osType} to reset and use production logic, or pass {@link OsType#UNKNOWN} to simulate the
     * case where no matching device exists.
     *
     * @param osType The test device {@link OsType}, {@link OsType#UNKNOWN} for no matching device,
     *     or {@code null} to reset to production code.
     * @param formFactor The test device {@link FormFactor}, or {@code null} (treated as {@link
     *     FormFactor#UNKNOWN}).
     */
    public static void setBestMatchDeviceForTesting(
            @Nullable @OsType Integer osType, @Nullable @FormFactor Integer formFactor) {
        @Nullable DeviceOsAndFormFactor oldState = sBestMatchDeviceForTesting;
        sBestMatchDeviceForTesting =
                osType == null
                        ? null
                        : new DeviceOsAndFormFactor(
                                osType, formFactor == null ? FormFactor.UNKNOWN : formFactor);
        ResettersForTesting.register(
                () -> {
                    sBestMatchDeviceForTesting = oldState;
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

        @JniType("std::vector<int32_t>")
        int[] getBestMatchDeviceOsTypeAndFormFactor(long profile, long crossDevicePrefTracker);
    }
}
