// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.prefs;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.sync_preferences.cross_device_pref_tracker.CrossDevicePrefTracker;

/**
 * Java-side counterpart of {@link CrossDevicePrefTrackerFactory}, used to get an instance for a
 * profile.
 */
@NullMarked
public class CrossDevicePrefTrackerFactory {
    private static @Nullable CrossDevicePrefTracker sCrossDevicePrefTrackerForTest;
    private static boolean sIsOverriddenForTest;

    private CrossDevicePrefTrackerFactory() {}

    /**
     * Retrieves or creates the CrossDevicePrefTracker associated with the specified Profile.
     * Returns null for off-the-record profiles and if sync is disabled (via flag or variation).
     *
     * <p>Can only be accessed on the main thread.
     *
     * @param profile The profile associated the CrossDevicePrefTracker being fetched.
     * @return The CrossDevicePrefTracker (if any) associated with the Profile.
     */
    public static @Nullable CrossDevicePrefTracker getForProfile(Profile profile) {
        ThreadUtils.assertOnUiThread();
        if (sIsOverriddenForTest) return sCrossDevicePrefTrackerForTest;
        if (profile == null) {
            throw new IllegalArgumentException(
                    "Attempting to access the CrossDevicePrefTracker with a null profile");
        }
        profile.ensureNativeInitialized();
        return CrossDevicePrefTrackerFactoryJni.get().getForProfile(profile);
    }

    /**
     * Overrides the initialization for tests. The tests should call resetForTests() at shutdown.
     */
    public static void setInstanceForTesting(@Nullable CrossDevicePrefTracker tracker) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sCrossDevicePrefTrackerForTest = tracker;
                    sIsOverriddenForTest = true;
                });
        ResettersForTesting.register(
                () -> {
                    sCrossDevicePrefTrackerForTest = null;
                    sIsOverriddenForTest = false;
                });
    }

    @NativeMethods
    public interface Natives {
        CrossDevicePrefTracker getForProfile(@JniType("Profile*") Profile profile);
    }
}
