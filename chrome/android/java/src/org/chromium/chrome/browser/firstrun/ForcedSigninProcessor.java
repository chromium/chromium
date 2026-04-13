// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.prefs.LocalStatePrefs;
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
     */
    // TODO(bauerb): Once external dependencies reliably use policy to force sign-in,
    // consider removing the child account.
    public static void checkCanSignIn(final Activity activity) {
        // TODO(crbug.com/467707243): Replace this with {@link
        // ForcedSigninController.isForcedSigninPolicyEnabled()} once the feature flag is enabled by
        // default.
        if (assumeNonNull(LocalStatePrefs.get()).getBoolean(Pref.FORCE_BROWSER_SIGNIN)) {
            ExternalAuthUtils.getInstance()
                    .canUseGooglePlayServices(
                            new UserRecoverableErrorHandler.ModalDialog(activity, false));
        }
    }
}
