// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import androidx.annotation.Nullable;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

/** This factory creates TabGroupSyncService for the given {@link Profile}. */
public final class TabGroupSyncServiceFactory {
    private static TabGroupSyncService sTabGroupSyncServiceForTesting;

    /**
     * A factory method to create or retrieve a {@link TabGroupSyncService} object for a given
     * profile.
     *
     * @return The {@link TabGroupSyncService} for the given profile.
     */
    public static TabGroupSyncService getForProfile(Profile profile) {
        if (sTabGroupSyncServiceForTesting != null) {
            return sTabGroupSyncServiceForTesting;
        }

        assert !profile.isOffTheRecord();

        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_GROUP_SYNC_ANDROID)) {
            return null;
        }

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
