// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.content.Context;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.omaha.UpdateStatusProvider;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;

/** A delegate for Safety Hub to handle UI related behaviour. */
public interface SafetyHubModuleDelegate {

    /**
     * @return A boolean indicating whether to show the account-level password check module in
     *     Safety Hub based on the Sync and UPM status.
     */
    boolean shouldShowPasswordCheckModule();

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
     * @return The current safe browsing state.
     */
    @SafeBrowsingState
    int getSafeBrowsingState();

    /**
     * @return Whether the Safe Browsing preference is managed.
     */
    boolean isSafeBrowsingManaged();
}
