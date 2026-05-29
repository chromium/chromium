// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.os.Build;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.DeviceInfo;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.OtrProfileId;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKey;
import org.chromium.chrome.browser.profiles.ProfileKeyUtil;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.util.MultiInstanceUtils;
import org.chromium.ui.display.DisplayUtil;

/** Utilities for working with incognito tabs spread across multiple activities. */
@NullMarked
public class IncognitoUtils {
    private static final double LARGE_DIAGONAL_DISPLAY_THRESHOLD_INCHES = 8.0;
    private static final double RESTRICTED_LARGE_DIAGONAL_DISPLAY_THRESHOLD_INCHES = 10.0;
    // Test emulators run as ~8" or ~9" tablets. We need a lower threshold of 8.0" to allow tests to
    // execute incognito multi-window flows without being blocked by the 10.0" production limit.
    private static final double LARGE_DIAGONAL_DISPLAY_THRESHOLD_INCHES_FOR_TEST = 8.0;
    private static @Nullable Boolean sIsEnabledForTesting;
    private static @Nullable Boolean sShouldOpenIncognitoAsWindowForTesting;

    private IncognitoUtils() {}

    /**
     * @param profile The {@link Profile} used to determine incognito status.
     * @return Whether incognito mode is enabled.
     */
    public static boolean isIncognitoModeEnabled(Profile profile) {
        if (sIsEnabledForTesting != null) {
            return sIsEnabledForTesting;
        }
        return IncognitoUtilsJni.get().getIncognitoModeEnabled(profile);
    }

    /**
     * @param profile The {@link Profile} used to determine incognito status.
     * @return Whether incognito mode is managed by policy.
     */
    public static boolean isIncognitoModeManaged(Profile profile) {
        return IncognitoUtilsJni.get().getIncognitoModeManaged(profile);
    }

    /**
     * Returns the {@link ProfileKey} from given {@link OtrProfileId}. If OtrProfileId is null, it
     * is the key of regular profile.
     *
     * @param otrProfileId The {@link OtrProfileId} of the profile. Null for regular profile.
     * @return The {@link ProfileKey} of the key.
     */
    public static ProfileKey getProfileKeyFromOtrProfileId(@Nullable OtrProfileId otrProfileId) {
        // If off-the-record is not requested, the request might be before native initialization.
        if (otrProfileId == null) return ProfileKeyUtil.getLastUsedRegularProfileKey();

        Profile profile =
                ProfileManager.getLastUsedRegularProfile()
                        .getOffTheRecordProfile(otrProfileId, /* createIfNeeded= */ true);
        assumeNonNull(profile);
        return profile.getProfileKey();
    }

    public static void setEnabledForTesting(Boolean enabled) {
        sIsEnabledForTesting = enabled;
        ResettersForTesting.register(() -> sIsEnabledForTesting = null);
    }

    /**
     * @return Whether incognito tabs should open in a separate window.
     */
    public static boolean shouldOpenIncognitoAsWindow() {
        if (!ChromeFeatureList.sAndroidOpenIncognitoAsWindow.isEnabled()) {
            return false;
        }
        // Honor test override first. This is needed by Unit Test.
        if (sShouldOpenIncognitoAsWindowForTesting != null) {
            return sShouldOpenIncognitoAsWindowForTesting;
        }
        // Automotive is currently restricted to a single window.
        // The form factor check must happen before the display size check; because Automotive and
        // Foldable could also be tablet-sized.
        if (DeviceInfo.isAutomotive() || DeviceInfo.isFoldable()) {
            return false;
        }
        if (BuildConfig.IS_FOR_TEST) {
            // The feature should be ON for Android Desktop.
            // The screen size check is not reliable on Android Desktop emulator.
            sShouldOpenIncognitoAsWindowForTesting =
                    DeviceInfo.isDesktop()
                            || ThreadUtils.runOnUiThreadBlocking(
                                    () ->
                                            DisplayUtil.isGlobalDefaultDisplayWithMinDiagonal(
                                                    LARGE_DIAGONAL_DISPLAY_THRESHOLD_INCHES_FOR_TEST));
            return sShouldOpenIncognitoAsWindowForTesting;
        }

        // Simplified check based on MultiWindowUtils#isMultiInstanceApi31Enabled. Skips the
        // Manifest launchMode check due to dependency restrictions on ChromeTabbedActivity.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S_V2) {
            return false;
        }
        if (ChromeFeatureList.sAndroidOpenIncognitoAsWindowRestrictions.isEnabled()) {
            return !MultiInstanceUtils.isLowMemoryDevice()
                    && DisplayUtil.isGlobalDefaultDisplayWithMinDiagonal(
                            RESTRICTED_LARGE_DIAGONAL_DISPLAY_THRESHOLD_INCHES);
        }
        return DisplayUtil.isGlobalDefaultDisplayWithMinDiagonal(
                LARGE_DIAGONAL_DISPLAY_THRESHOLD_INCHES);
    }

    /**
     * @return Whether different model windows should open in full-screen.
     */
    public static boolean isIncognitoAsWindowFullScreenEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.INCOGNITO_AS_WINDOW_FULL_SCREEN)
                && shouldOpenIncognitoAsWindow();
    }

    /**
     * Sets the value returned by {@link #shouldOpenIncognitoAsWindow()} for testing.
     *
     * @param enabled The value to force, or null to revert to default behavior.
     */
    public static void setShouldOpenIncognitoAsWindowForTesting(Boolean enabled) {
        sShouldOpenIncognitoAsWindowForTesting = enabled;
        ResettersForTesting.register(() -> sShouldOpenIncognitoAsWindowForTesting = null);
    }

    /**
     * @return Whether incognito theme overlay is enabled for testing on current window.
     */
    public static boolean isIncognitoThemeOverlayEnabledForTesting() {
        return ChromeFeatureList.sIncognitoThemeOverlayTesting.isEnabled();
    }

    @NativeMethods
    public interface Natives {
        boolean getIncognitoModeEnabled(@JniType("Profile*") Profile profile);

        boolean getIncognitoModeManaged(@JniType("Profile*") Profile profile);
    }
}
