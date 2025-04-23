// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

/** This factory creates TabGroupSyncService for the given {@link Profile}. */
@NullMarked
public final class TabGroupSyncServiceFactory {
    private static @Nullable TabGroupSyncService sTabGroupSyncServiceForTesting;

    /**
     * A factory method to create or retrieve a {@link TabGroupSyncService} object for a given
     * profile.
     *
     * @return The {@link TabGroupSyncService} for the given profile.
     */
    public static @Nullable TabGroupSyncService getForProfile(Profile profile) {
        // Assert this before returning test value so that incorrect usage can be caught in tests.
        assert !profile.isOffTheRecord();
        if (sTabGroupSyncServiceForTesting != null) {
            return sTabGroupSyncServiceForTesting;
        }

        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_GROUP_SYNC_ANDROID)) {
            return null;
        }

        // Throw an exception if the native pointer is not initialized. This is useful to get a more
        // debuggable stacktrace than failing in native.
        profile.ensureNativeInitialized();
        return TabGroupSyncServiceFactoryJni.get().getForProfile(profile);
    }

    /**
     * Set a {@TabGroupSyncService} to use for testing. All subsequent calls to {@link
     * #getForProfile( Profile )} will return the test object rather than the real object.
     *
     * @param testService The {@TabGroupSyncService} to use for testing, or null if the real service
     *     should be used.
     */
    public static void setForTesting(@Nullable TabGroupSyncService testService) {
        sTabGroupSyncServiceForTesting = testService;
        ResettersForTesting.register(() -> sTabGroupSyncServiceForTesting = null);
    }

    private TabGroupSyncServiceFactory() {}

    @NativeMethods
    interface Natives {
        TabGroupSyncService getForProfile(@JniType("Profile*") Profile profile);
    }
}
