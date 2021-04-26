// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import androidx.annotation.MainThread;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncController;
import org.chromium.chrome.browser.sync.SyncErrorNotifier;

/**
 * This class is used to get a singleton instance of {@link SigninChecker}.
 */
public class SigninHelperProvider {
    private static SigninChecker sInstance;

    /**
     * @return A singleton instance of {@link SigninChecker}.
     *
     * TODO(crbug/1198038): Rename this class to SigninCheckerProvider
     */
    @MainThread
    public static SigninChecker get() {
        if (sInstance == null) {
            // SyncController and SyncErrorNotifier must be explicitly initialized.
            // TODO(crbug.com/1156620): Move the initializations elsewhere.
            SyncErrorNotifier.get();
            SyncController.get();
            Profile profile = Profile.getLastUsedRegularProfile();
            sInstance = new SigninChecker(IdentityServicesProvider.get().getSigninManager(profile),
                    IdentityServicesProvider.get().getAccountTrackerService(profile));
        }
        return sInstance;
    }
}
