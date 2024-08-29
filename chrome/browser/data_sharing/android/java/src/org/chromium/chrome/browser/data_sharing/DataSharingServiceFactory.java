// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import androidx.annotation.Nullable;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.data_sharing.DataSharingService;

/** This factory creates DataSharingService for the given {@link Profile}. */
public final class DataSharingServiceFactory {
    private static DataSharingService sDataSharingServiceForTesting;

    // Don't instantiate me.
    private DataSharingServiceFactory() {}

    /**
     * A factory method to create or retrieve a {@link DataSharingService} object for a given
     * profile.
     *
     * @return The {@link DataSharingService} for the given profile.
     */
    public static DataSharingService getForProfile(Profile profile) {
        if (sDataSharingServiceForTesting != null) {
            return sDataSharingServiceForTesting;
        }

        return DataSharingServiceFactoryJni.get().getForProfile(profile);
    }

    /**
     * Set a {@DataSharingService} to use for testing. All subsequent calls to {@link
     * #getForProfile( Profile )} will return the test object rather than the real object.
     *
     * @param testService The {@DataSharingService} to use for testing, or null if the real service
     *     should be used.
     */
    public static void setForTesting(@Nullable DataSharingService testService) {
        sDataSharingServiceForTesting = testService;
        ResettersForTesting.register(() -> sDataSharingServiceForTesting = null);
    }

    @NativeMethods
    interface Natives {
        DataSharingService getForProfile(@JniType("Profile*") Profile profile);
    }
}
