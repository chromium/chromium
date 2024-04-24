// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.content.Context;

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
}
