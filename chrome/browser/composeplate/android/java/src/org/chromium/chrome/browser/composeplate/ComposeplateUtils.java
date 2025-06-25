// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.composeplate;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.LocaleUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;

/** Utility class for the composeplate view. */
@NullMarked
public class ComposeplateUtils {
    private static final String LOCALE_US = "US";

    /**
     * Returns whether the composeplate can be enabled.
     *
     * @param isTablet Whether the device is a tablet.
     * @param profile The current profile.
     */
    public static boolean isComposeplateEnabled(boolean isTablet, Profile profile) {
        return ChromeFeatureList.sAndroidComposeplate.isEnabled()
                && !isTablet
                && (ChromeFeatureList.sAndroidComposeplateSkipLocaleCheck.getValue()
                        || LocaleUtils.getDefaultCountryCode().equals(LOCALE_US))
                && ComposeplateUtilsJni.get().isEnabledByPolicy(profile);
    }

    @NativeMethods
    @VisibleForTesting
    public interface Natives {
        boolean isEnabledByPolicy(@JniType("Profile*") Profile profile);
    }
}
