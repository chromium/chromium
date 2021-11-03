// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.password_manager;

/**
 * Interface for the launcher responsible for opening the Credential Manager.
 */
public interface CredentialManagerLauncher {
    /**
     * Launches the UI surface allowing users to manage their saved passwords.
     *
     * @param referrer the place that requested the launch
     */
    void launchCredentialManager(@ManagePasswordsReferrer int referrer);
}
