// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.content.Context;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.omaha.UpdateStatusProvider;
import org.chromium.chrome.browser.password_manager.PasswordStoreBridge;

/** A delegate for Safety Hub to handle UI related behaviour. */
public interface SafetyHubModuleDelegate {

    /**
     * Launches the Password Checkup UI from GMSCore.
     *
     * @param context used to show the dialog.
     */
    void showPasswordCheckUI(Context context);

    /**
     * @return The last fetched update status from Omaha if available.
     */
    @Nullable
    UpdateStatusProvider.UpdateStatus getUpdateStatus();

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
     * Open the sign-in bottomsheet or sync promo page based on {@link
     * ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS} state.
     *
     * @param context used to launch the promo in.
     */
    void launchSyncOrSigninPromo(Context context);
}
