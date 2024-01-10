// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKey;

/** Utilities for working with incognito tabs spread across multiple activities. */
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
                .getOffTheRecordProfile(otrProfileID, /* createIfNeeded= */ true)
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
