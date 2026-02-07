// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.synced_set_up;

import org.jni_zero.JNINamespace;
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
    }
}
