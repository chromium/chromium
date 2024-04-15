// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import androidx.annotation.Nullable;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.sync.SyncService;

/** Provides profile specific SyncService instances. */
public class SyncServiceFactory {
    @Nullable private static SyncService sSyncServiceForTest;

    private SyncServiceFactory() {}

    /**
     * Retrieves or creates the SyncService associated with the specified Profile. Returns null for
     * off-the-record profiles and if sync is disabled (via flag or variation).
     *
     * Can only be accessed on the main thread.
     *
     * @param profile The profile associated the SyncService being fetched.
     * @return The SyncService (if any) associated with the Profile.
     */
    public static @Nullable SyncService getForProfile(Profile profile) {
        ThreadUtils.assertOnUiThread();
        if (sSyncServiceForTest != null) return sSyncServiceForTest;
        if (profile == null) {
            throw new IllegalArgumentException(
                    "Attempting to access the SyncService with a null profile");
        }
        profile.ensureNativeInitialized();
        return SyncServiceFactoryJni.get().getForProfile(profile);
    }

    /**
     * Overrides the initialization for tests. The tests should call resetForTests() at shutdown.
     */
    public static void setInstanceForTesting(SyncService syncService) {
        ThreadUtils.runOnUiThreadBlocking((Runnable) () -> sSyncServiceForTest = syncService);
        ResettersForTesting.register(() -> sSyncServiceForTest = null);
    }

    @NativeMethods
    interface Natives {
        SyncService getForProfile(@JniType("Profile*") Profile profile);
    }
}
