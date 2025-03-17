// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.regional_capabilities;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.regional_capabilities.RegionalCapabilitiesService;

/**
 * This factory links the native RegionalCapabilitiesService for the current Profile to create and
 * hold a {@link RegionalCapabilitiesService} instance.
 */
@JNINamespace("regional_capabilities")
@NullMarked
public class RegionalCapabilitiesServiceFactory {
    private static @Nullable RegionalCapabilitiesService sRegionalCapabilitiesServiceForTesting;

    private RegionalCapabilitiesServiceFactory() {}

    /**
     * Retrieve the RegionalCapabilitiesService for a given profile.
     *
     * @param profile The profile associated with the RegionalCapabilitiesService.
     * @return The profile specific RegionalCapabilitiesService.
     */
    public static RegionalCapabilitiesService getForProfile(Profile profile) {
        ThreadUtils.assertOnUiThread();
        if (sRegionalCapabilitiesServiceForTesting != null) {
            return sRegionalCapabilitiesServiceForTesting;
        }
        return RegionalCapabilitiesServiceFactoryJni.get().getRegionalCapabilitiesService(profile);
    }

    public static void setInstanceForTesting(RegionalCapabilitiesService service) {
        sRegionalCapabilitiesServiceForTesting = service;
        ResettersForTesting.register(() -> sRegionalCapabilitiesServiceForTesting = null);
    }

    // Natives interface is public to allow mocking in tests outside of
    // org.chromium.chrome.browser.regional_capabilities package.
    @NativeMethods
    public interface Natives {
        RegionalCapabilitiesService getRegionalCapabilitiesService(
                @JniType("Profile*") Profile profile);
    }
}
