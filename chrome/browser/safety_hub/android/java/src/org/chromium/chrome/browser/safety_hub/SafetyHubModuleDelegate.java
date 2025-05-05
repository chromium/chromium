// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.password_manager.PasswordStoreBridge;

/** A delegate for Safety Hub to handle UI related behaviour. */
@NullMarked
public interface SafetyHubModuleDelegate {

    /**
     * Launches the Password Checkup UI from GMSCore.
     *
     * @param context used to show the dialog.
     */
    // TODO(crbug.com/388788969): Rename to `showAccountPasswordCheckUi`.
    void showPasswordCheckUi(Context context);

    /**
     * Launches the Local Password Checkup UI from GMSCore.
     *
     * @param context used to show the dialog.
     */
    void showLocalPasswordCheckUi(Context context);

    /**
     * Opens the Play Store page for the installed Chrome channel.
     *
     * @param context used to launch the play store intent.
     */
    void openGooglePlayStore(Context context);

    /**
     * @param passwordStoreBridge Provides access to stored passwords.
     * @return the total passwords count for Account-level passwords.
     */
    int getAccountPasswordsCount(@Nullable PasswordStoreBridge passwordStoreBridge);

    /**
     * @param passwordStoreBridge Provides access to stored passwords.
     * @return the total passwords count for local-level passwords.
     */
    int getLocalPasswordsCount(@Nullable PasswordStoreBridge passwordStoreBridge);

    /**
     * Opens the sign-in bottomsheet.
     *
     * @param context used to launch the promo in.
     */
    void launchSigninPromo(Context context);
}
