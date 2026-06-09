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

    /** Returns true if the user was previously determined to be ineligible for Glic. */
    public static boolean wasPreviouslyNotAllowed(@Nullable Profile profile) {
        if (profile == null) return false;
        if (sIsEnabledForTesting != null) return sIsEnabledForTesting;
        return GlicEnablingJni.get().wasPreviouslyNotAllowed(profile);
    }

    /** Returns true if the Glic settings page should be shown for the given profile. */
    public static boolean shouldShowSettingsPage(@Nullable Profile profile) {
        if (profile == null) return false;
        if (sIsEnabledForTesting != null) return sIsEnabledForTesting;
        return GlicEnablingJni.get().shouldShowSettingsPage(profile);
    }

    /** Returns true if the web actuation / auto browse toggle should be shown for the profile. */
    public static boolean shouldShowWebActuationToggle(@Nullable Profile profile) {
        if (profile == null) return false;
        if (sIsEnabledForTesting != null) return sIsEnabledForTesting;
        return GlicEnablingJni.get().shouldShowWebActuationToggle(profile);
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
     *
     * <p>Note: This only sets up the current state for java testing. To forward the value to native
     * side during browser test, use {@link #setEnabledForTesting(boolean, boolean)}
     *
     * @see #setEnabledForTesting(boolean, boolean)
     */
    public static void setEnabledForTesting(boolean isEnabled) {
        setEnabledForTesting(isEnabled, /* forwardToNative= */ false);
    }

    /**
     * Sets the global Glic feature to the given value for testing. This value will override the
     * value returned by all eligible checks in this file.
     *
     * <p>Note: When calling this method in unit tests, ensure that native JNI mocks are properly
     * set up (e.g. via {@link GlicEnablingJni#setInstanceForTesting}) to avoid native resolution
     * errors.
     */
    public static void setEnabledForTesting(boolean isEnabled, boolean forwardToNative) {
        sIsEnabledForTesting = isEnabled;
        if (forwardToNative && isEnabled) {
            GlicEnablingJni.get().setBypassEnablementChecksForTesting(true);
        }
        ResettersForTesting.register(
                () -> {
                    sIsEnabledForTesting = null;
                    if (forwardToNative) {
                        GlicEnablingJni.get().setBypassEnablementChecksForTesting(false);
                    }
                });
    }

    /** Returns true if Glic is disabled by policy for the given profile. */
    public static boolean isDisabledByPolicy(@Nullable Profile profile) {
        if (profile == null) return false;
        return GlicEnablingJni.get().isDisabledByPolicy(profile);
    }

    /** Returns true if the profile is managed by enterprise. */
    public static boolean isProfileManaged(@Nullable Profile profile) {
        if (profile == null) return false;
        return GlicEnablingJni.get().isProfileManaged(profile);
    }

    @NativeMethods
    public interface Natives {
        boolean isEnabledByFlags();

        boolean isProfileEligible(@JniType("Profile*") Profile profile);

        boolean isEnabledForProfile(@JniType("Profile*") Profile profile);

        boolean wasPreviouslyNotAllowed(@JniType("Profile*") Profile profile);

        boolean shouldShowSettingsPage(@JniType("Profile*") Profile profile);

        boolean shouldShowWebActuationToggle(@JniType("Profile*") Profile profile);

        boolean isReadyForProfile(@JniType("Profile*") Profile profile);

        boolean isDisabledByPolicy(@JniType("Profile*") Profile profile);

        boolean isProfileManaged(@JniType("Profile*") Profile profile);

        void setBypassEnablementChecksForTesting(boolean bypass);
    }
}
