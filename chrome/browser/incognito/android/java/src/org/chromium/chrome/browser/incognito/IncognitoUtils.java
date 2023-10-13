// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import androidx.annotation.Nullable;

import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKey;
import org.chromium.ui.base.WindowAndroid;

/**
 * Utilities for working with incognito tabs spread across multiple activities.
 */
public class IncognitoUtils {
    private static Boolean sIsEnabledForTesting;

    private IncognitoUtils() {}

    /**
     * @return true if incognito mode is enabled.
     */
    public static boolean isIncognitoModeEnabled() {
        if (sIsEnabledForTesting != null) {
            return sIsEnabledForTesting;
        }
        return IncognitoUtilsJni.get().getIncognitoModeEnabled();
    }

    /**
     * @return true if incognito mode is managed by policy.
     */
    public static boolean isIncognitoModeManaged() {
        return IncognitoUtilsJni.get().getIncognitoModeManaged();
    }

    /**
     * Returns either a regular profile or a (primary/non-primary) Incognito profile.
     *
     * <p>
     * Note, {@link WindowAndroid} is keyed only to non-primary Incognito profile, in default cases
     * primary Incognito profile would be returned.
     * <p>
     *
     * @param windowAndroid {@link WindowAndroid} object.
     * @param isIncognito A boolean to indicate if an Incognito profile should be fetched.
     *
     * @return A regular {@link Profile} object if |isIncognito| is false or an Incognito {@link
     *         Profile} object otherwise.
     */
    public static Profile getProfileFromWindowAndroid(
            WindowAndroid windowAndroid, boolean isIncognito) {
        if (!isIncognito) return Profile.getLastUsedRegularProfile();
        return getIncognitoProfileFromWindowAndroid(windowAndroid);
    }

    /**
     * Returns either the non-primary OTR profile if any that is associated with a |windowAndroid|
     * instance, otherwise the primary OTR profile.
     * <p>
     * A non primary OTR profile is associated only for the case of incognito CustomTabActivity.
     * <p>
     * @param windowAndroid The {@link WindowAndroid} instance for which the non primary OTR
     *         profile is queried.
     *
     * @return A non-primary or a primary OTR {@link Profile}.
     */
    public static Profile getIncognitoProfileFromWindowAndroid(
            @Nullable WindowAndroid windowAndroid) {
        Profile incognitoProfile = getNonPrimaryOTRProfileFromWindowAndroid(windowAndroid);
        return (incognitoProfile != null)
                ? incognitoProfile
                : Profile.getLastUsedRegularProfile().getPrimaryOTRProfile(/*createIfNeeded=*/true);
    }

    /**
     * Returns the non primary OTR profile if any that is associated with a |windowAndroid|
     * instance, otherwise null.
     * <p>
     * A non primary OTR profile is associated only for the case of incognito CustomTabActivity.
     * <p>
     * @param windowAndroid The {@link WindowAndroid} instance for which the non primary OTR
     *         profile is queried.
     */
    public static @Nullable Profile getNonPrimaryOTRProfileFromWindowAndroid(
            @Nullable WindowAndroid windowAndroid) {
        if (windowAndroid == null) return null;

        IncognitoCctProfileManager incognitoCctProfileManager =
                IncognitoCctProfileManager.from(windowAndroid);

        if (incognitoCctProfileManager == null) return null;
        return incognitoCctProfileManager.getProfile();
    }

    /**
     * Returns the {@link ProfileKey} from given {@link OTRProfileID}. If OTRProfileID is null, it
     * is the key of regular profile.
     *
     * @param otrProfileID The {@link OTRProfileID} of the profile. Null for regular profile.
     * @return The {@link ProfileKey} of the key.
     */
    public static ProfileKey getProfileKeyFromOTRProfileID(OTRProfileID otrProfileID) {
        // If off-the-record is not requested, the request might be before native initialization.
        if (otrProfileID == null) return ProfileKey.getLastUsedRegularProfileKey();

        return Profile.getLastUsedRegularProfile()
                .getOffTheRecordProfile(otrProfileID, /*createIfNeeded=*/true)
                .getProfileKey();
    }

    public static void setEnabledForTesting(Boolean enabled) {
        sIsEnabledForTesting = enabled;
        ResettersForTesting.register(() -> sIsEnabledForTesting = null);
    }

    @NativeMethods
    interface Natives {
        boolean getIncognitoModeEnabled();
        boolean getIncognitoModeManaged();
    }
}
