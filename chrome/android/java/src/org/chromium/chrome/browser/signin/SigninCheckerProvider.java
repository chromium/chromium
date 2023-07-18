// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import androidx.annotation.MainThread;

import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninChecker;
import org.chromium.chrome.browser.sync.SyncErrorNotifier;
import org.chromium.chrome.browser.sync.SyncServiceFactory;

/**
 * This class is used to get a singleton instance of {@link SigninChecker}.
 */
public final class SigninCheckerProvider {
    private static SigninChecker sInstance;

    /**
     * @return A singleton instance of {@link SigninChecker}.
     */
    @MainThread
    public static SigninChecker get() {
        if (sInstance == null) {
            // SyncErrorNotifier must be explicitly initialized.
            // TODO(crbug.com/1156620): Move the initializations elsewhere.
            SyncErrorNotifier.get();
            Profile profile = Profile.getLastUsedRegularProfile();
            sInstance = new SigninChecker(IdentityServicesProvider.get().getSigninManager(profile),
                    IdentityServicesProvider.get().getAccountTrackerService(profile),
                    SyncServiceFactory.getForProfile(profile));
        }
        return sInstance;
    }

    @MainThread
    public static void setForTests(SigninChecker signinChecker) {
        var oldValue = sInstance;
        sInstance = signinChecker;
        ResettersForTesting.register(() -> sInstance = oldValue);
    }

    private SigninCheckerProvider() {}
}
