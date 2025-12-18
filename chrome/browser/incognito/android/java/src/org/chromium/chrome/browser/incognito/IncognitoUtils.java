// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.Context;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.OtrProfileId;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKey;
import org.chromium.chrome.browser.profiles.ProfileKeyUtil;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.ui.base.DeviceFormFactor;

/** Utilities for working with incognito tabs spread across multiple activities. */
@NullMarked
public class IncognitoUtils {
    private static @Nullable Boolean sIsEnabledForTesting;
    private static @Nullable Boolean sIsEligibleTablet;

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
        // TODO(crbug.com/467768341): Clean up the desktop and tablet form factor check once the bug
        // is fixed.
        if (DeviceInfo.isDesktop()) {
            return true;
        }
        // Automotive is currently restricted to a single window.
        if (DeviceInfo.isAutomotive()) {
            return false;
        }

        boolean isTablet;
        if (sIsEligibleTablet != null) {
            isTablet = sIsEligibleTablet;
        } else {
            isTablet =
                    DeviceFormFactor.isNonMultiDisplayContextOnTablet(
                            ContextUtils.getApplicationContext());
        }
        return isTablet;
    }

    /**
     * @return Whether incognito theme overlay is enabled for testing on current window.
     */
    public static boolean isIncognitoThemeOverlayEnabledForTesting() {
        return ChromeFeatureList.sIncognitoThemeOverlayTesting.isEnabled();
    }

    /**
     * Initialize {@code sIsEligibleTablet} status if not already.
     *
     * @param context {@link Activity} context used to determine if the display is tablet size.
     * @param isMultiInstanceApi31Enabled Whether the new launch mode 'singleInstancePerTask' is
     *     configured to allow multiple instantiation of Chrome instance. The device is not eligible
     *     tablet if multiple instantiation of Chrome instance is not allowed.
     */
    public static void initializeEligibleTabletStatus(
            Context context, boolean isMultiInstanceApi31Enabled) {
        if (sIsEligibleTablet != null) {
            return;
        }
        if (!isMultiInstanceApi31Enabled
                || !ChromeFeatureList.sAndroidOpenIncognitoAsWindow.isEnabled()) {
            sIsEligibleTablet = false;
        } else {
            sIsEligibleTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(context);
        }
    }

    @NativeMethods
    public interface Natives {
        boolean getIncognitoModeEnabled(@JniType("Profile*") Profile profile);

        boolean getIncognitoModeManaged(@JniType("Profile*") Profile profile);
    }
}
