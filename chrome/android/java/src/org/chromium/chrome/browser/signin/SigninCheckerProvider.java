// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import androidx.annotation.MainThread;

import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninChecker;
import org.chromium.chrome.browser.sync.SyncErrorNotifier;

/** This class is used to get a singleton instance of {@link SigninChecker}. */
public final class SigninCheckerProvider {
    private static ProfileKeyedMap<SigninChecker> sProfileMap =
            ProfileKeyedMap.createMapOfDestroyables();
    private static SigninChecker sInstanceForTesting;

    /**
     * @return A {@link SigninChecker} instance for the given Profile.
     */
    @MainThread
    public static SigninChecker get(Profile profile) {
        if (sInstanceForTesting != null) return sInstanceForTesting;
        return sProfileMap.getForProfile(profile, SigninCheckerProvider::buildForProfile);
    }

    private static SigninChecker buildForProfile(Profile profile) {
        // SyncErrorNotifier must be explicitly initialized.
        // TODO(crbug.com/40736034): Move the initializations elsewhere.
        SyncErrorNotifier.getForProfile(profile);
        return new SigninChecker(IdentityServicesProvider.get().getSigninManager(profile));
    }

    @MainThread
    public static void setForTests(SigninChecker signinChecker) {
        var oldValue = sInstanceForTesting;
        sInstanceForTesting = signinChecker;
        ResettersForTesting.register(() -> sInstanceForTesting = oldValue);
    }

    private SigninCheckerProvider() {}
}
