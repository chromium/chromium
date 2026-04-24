// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;

/** Java counterpart for {@link glic::GlicEnabling}. */
@JNINamespace("glic")
@NullMarked
public class GlicEnabling {

    private static @Nullable Boolean sIsEnabledForTesting;

    /**
     * Returns whether the global Glic feature is enabled for Chrome. This status will not change at
     * runtime.
     */
    public static boolean isEnabledByFlags() {
        if (sIsEnabledForTesting != null) return sIsEnabledForTesting;
        return GlicEnablingJni.get().isEnabledByFlags();
    }

    /**
     * Returns true if a profile is eligible for Glic, that is, it can potentially be enabled,
     * regardless of whether it is currently enabled or not. Always returns false if {@link
     * #isEnabledByFlags()} is off. This will never change for a given profile.
     */
    public static boolean isProfileEligible(@Nullable Profile profile) {
        if (profile == null) return false;
        if (sIsEnabledForTesting != null) return sIsEnabledForTesting;
        return GlicEnablingJni.get().isProfileEligible(profile);
    }

    /** Returns true if the given profile has Glic enabled. This value can change at runtime. */
    public static boolean isEnabledForProfile(@Nullable Profile profile) {
        if (profile == null) return false;
        if (sIsEnabledForTesting != null) return sIsEnabledForTesting;
        return GlicEnablingJni.get().isEnabledForProfile(profile);
    }

    /** Returns true if the Glic settings page should be shown for the given profile. */
    public static boolean shouldShowSettingsPage(@Nullable Profile profile) {
        if (profile == null) return false;
        if (sIsEnabledForTesting != null) return sIsEnabledForTesting;
        return GlicEnablingJni.get().shouldShowSettingsPage(profile);
    }

    /** Returns true if Glic is ready to be used for the given profile. */
    public static boolean isReadyForProfile(@Nullable Profile profile) {
        if (profile == null) return false;
        if (sIsEnabledForTesting != null) return sIsEnabledForTesting;
        return GlicEnablingJni.get().isReadyForProfile(profile);
    }

    /**
     * Sets the global Glic feature to the given value for testing. This value will override the
     * value returned by all eligible checks in this file.
     */
    public static void setEnabledForTesting(boolean isEnabled) {
        sIsEnabledForTesting = isEnabled;
        ResettersForTesting.register(() -> sIsEnabledForTesting = null);
    }

    @NativeMethods
    public interface Natives {
        boolean isEnabledByFlags();

        boolean isProfileEligible(@JniType("Profile*") Profile profile);

        boolean isEnabledForProfile(@JniType("Profile*") Profile profile);

        boolean shouldShowSettingsPage(@JniType("Profile*") Profile profile);

        boolean isReadyForProfile(@JniType("Profile*") Profile profile);
    }
}
