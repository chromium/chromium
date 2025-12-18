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
    private static @Nullable Boolean sIsTablet;

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

        boolean isTablet;
        if (sIsTablet != null) {
            isTablet = sIsTablet;
        } else {
            isTablet =
                    DeviceFormFactor.isNonMultiDisplayContextOnTablet(
                                    ContextUtils.getApplicationContext())
                            && !DeviceInfo.isAutomotive();
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
     * Initialize {@code sIsTablet} status if not already.
     *
     * @param context {@link Activity} context used to determine if the display is tablet size.
     */
    public static void initializeTabletStatus(Context context) {
        if (sIsTablet != null || !ChromeFeatureList.sAndroidOpenIncognitoAsWindow.isEnabled()) {
            return;
        }
        sIsTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(context);
    }

    @NativeMethods
    public interface Natives {
        boolean getIncognitoModeEnabled(@JniType("Profile*") Profile profile);

        boolean getIncognitoModeManaged(@JniType("Profile*") Profile profile);
    }
}
