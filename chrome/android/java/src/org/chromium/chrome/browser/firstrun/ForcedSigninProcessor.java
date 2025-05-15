// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.externalauth.UserRecoverableErrorHandler;

/** A helper to perform all necessary steps for forced sign in. */
@NullMarked
public final class ForcedSigninProcessor {
    /*
     * Only for static usage.
     */
    private ForcedSigninProcessor() {}

    /**
     * If forced signin is required by policy, check that Google Play Services is available, and
     * show a non-cancelable dialog otherwise.
     *
     * @param activity The activity for which to show the dialog.
     * @param profile The currently used Profile.
     */
    // TODO(bauerb): Once external dependencies reliably use policy to force sign-in,
    // consider removing the child account.
    public static void checkCanSignIn(final Activity activity, Profile profile) {
        SigninManager signinManager =
                assumeNonNull(IdentityServicesProvider.get().getSigninManager(profile));
        if (signinManager.isForceSigninEnabled()) {
            ExternalAuthUtils.getInstance()
                    .canUseGooglePlayServices(
                            new UserRecoverableErrorHandler.ModalDialog(activity, false));
        }
    }
}
