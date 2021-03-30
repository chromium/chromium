// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import androidx.annotation.MainThread;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninHelper;
import org.chromium.chrome.browser.sync.SyncController;
import org.chromium.chrome.browser.sync.SyncErrorNotifier;

/**
 * This class is used to get a singleton instance of {@link SigninHelper}.
 */
public class SigninHelperProvider {
    private static SigninHelper sInstance;

    /**
     * @return A singleton instance of {@link SigninHelper}.
     */
    @MainThread
    public static SigninHelper get() {
        if (sInstance == null) {
            // SyncController and SyncErrorNotifier must be explicitly initialized.
            // TODO(crbug.com/1156620): Move the initializations elsewhere.
            SyncErrorNotifier.get();
            SyncController.get();
            Profile profile = Profile.getLastUsedRegularProfile();
            sInstance = new SigninHelper(IdentityServicesProvider.get().getSigninManager(profile),
                    IdentityServicesProvider.get().getAccountTrackerService(profile));
        }
        return sInstance;
    }
}
