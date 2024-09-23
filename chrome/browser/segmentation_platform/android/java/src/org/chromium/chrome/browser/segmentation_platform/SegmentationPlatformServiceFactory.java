// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.segmentation_platform;

import androidx.annotation.Nullable;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.segmentation_platform.SegmentationPlatformService;

/** This factory creates SegmentationPlatformService for the given {@link Profile}. */
public final class SegmentationPlatformServiceFactory {
    private static SegmentationPlatformService sSegmentationPlatformServiceForTesting;

    // Don't instantiate me.
    private SegmentationPlatformServiceFactory() {}

    /**
     * A factory method to create or retrieve a {@link SegmentationPlatformService} object for a
     * given profile.
     * @return The {@link SegmentationPlatformService} for the given profile.
     */
    public static SegmentationPlatformService getForProfile(Profile profile) {
        if (sSegmentationPlatformServiceForTesting != null) {
            return sSegmentationPlatformServiceForTesting;
        }

        return SegmentationPlatformServiceFactoryJni.get().getForProfile(profile);
    }

    /**
     * Set a {@SegmentationPlatformService} to use for testing. All subsequent calls to {@link
     * #getForProfile( Profile )} will return the test object rather than the real object.
     *
     * @param testService The {@SegmentationPlatformService} to use for testing, or null if the real
     *         service should be used.
     */
    public static void setForTests(@Nullable SegmentationPlatformService testService) {
        sSegmentationPlatformServiceForTesting = testService;
        ResettersForTesting.register(() -> sSegmentationPlatformServiceForTesting = null);
    }

    @NativeMethods
    interface Natives {
        SegmentationPlatformService getForProfile(@JniType("Profile*") Profile profile);
    }
}
