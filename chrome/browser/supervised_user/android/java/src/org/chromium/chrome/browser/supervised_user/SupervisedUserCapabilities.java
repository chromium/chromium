// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.supervised_user;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.signin.Tribool;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;

/** Utility class to determine if an account/profile is supervised using capabilities. */
public class SupervisedUserCapabilities {
    /**
     * Determines if a given profile is supervised based on the {@link
     * IS_SUBJECT_TO_PARENTAL_CONTROLS_CAPABILITY_NAME} capability.
     *
     * @param profile the profile to check for supervision status
     */
    public static boolean isSubjectToParentalControls(Profile profile) {
        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.REPLACE_PROFILE_IS_CHILD_WITH_ACCOUNT_CAPABILITIES_ON_ANDROID)) {
            IdentityManager identityManager =
                    IdentityServicesProvider.get().getIdentityManager(profile);
            if (identityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN) == null) {
                throw new IllegalStateException("AccountInfo is unavailable");
            }
            AccountInfo accountinfo =
                    IdentityServicesProvider.get()
                            .getIdentityManager(profile)
                            .findExtendedAccountInfoByEmailAddress(
                                    identityManager
                                            .getPrimaryAccountInfo(ConsentLevel.SIGNIN)
                                            .getEmail());
            return accountinfo.getAccountCapabilities().isSubjectToParentalControls()
                    == Tribool.TRUE;
        }
        return profile.isChild();
    }
}
