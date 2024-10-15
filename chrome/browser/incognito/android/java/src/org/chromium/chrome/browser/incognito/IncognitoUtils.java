// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.profiles.OtrProfileId;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKey;
import org.chromium.chrome.browser.profiles.ProfileKeyUtil;
import org.chromium.chrome.browser.profiles.ProfileManager;

/** Utilities for working with incognito tabs spread across multiple activities. */
public class IncognitoUtils {
    private static Boolean sIsEnabledForTesting;

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
    public static ProfileKey getProfileKeyFromOtrProfileId(OtrProfileId otrProfileId) {
        // If off-the-record is not requested, the request might be before native initialization.
        if (otrProfileId == null) return ProfileKeyUtil.getLastUsedRegularProfileKey();

        return ProfileManager.getLastUsedRegularProfile()
                .getOffTheRecordProfile(otrProfileId, /* createIfNeeded= */ true)
                .getProfileKey();
    }

    public static void setEnabledForTesting(Boolean enabled) {
        sIsEnabledForTesting = enabled;
        ResettersForTesting.register(() -> sIsEnabledForTesting = null);
    }

    @NativeMethods
    public interface Natives {
        boolean getIncognitoModeEnabled(@JniType("Profile*") Profile profile);

        boolean getIncognitoModeManaged(@JniType("Profile*") Profile profile);
    }
}
