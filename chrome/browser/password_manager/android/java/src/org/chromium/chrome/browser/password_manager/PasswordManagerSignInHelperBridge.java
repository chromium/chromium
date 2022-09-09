// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.app.Activity;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.ui.base.WindowAndroid;

/**
 *  The bridge provides a way to interact with the Android sign in flow.
 */
public class PasswordManagerSignInHelperBridge {
    /**
     * Starts the Android process to update credentials for the primary account in Chrome.
     * This method will only work for users that have been previously signed in Chrome on the
     * device.
     */
    @CalledByNative
    public static void startUpdateAccountCredentialsFlow(WindowAndroid windowAndroid) {
        Profile profile = Profile.getLastUsedRegularProfile();
        final CoreAccountInfo primaryAccountInfo =
                IdentityServicesProvider.get().getIdentityManager(profile).getPrimaryAccountInfo(
                        ConsentLevel.SIGNIN);
        // It's not possible to call updateCredentials without an account.
        assert primaryAccountInfo != null;
        final Activity activity = windowAndroid.getActivity().get();
        AccountManagerFacadeProvider.getInstance().updateCredentials(
                CoreAccountInfo.getAndroidAccountFrom(primaryAccountInfo), activity, null);
    }
}
