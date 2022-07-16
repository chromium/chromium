// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.password_manager;

import android.app.PendingIntent;

import org.chromium.base.Callback;

/**
 * Interface for the launcher responsible for opening the Credential Manager.
 */
public interface CredentialManagerLauncher {
    /**
     * Launches the UI surface allowing users to manage their saved passwords.
     *
     * TODO(crbug.com/1255038): Remove once it becomes unused.
     *
     * @param referrer the place that requested the launch
     */
    @Deprecated
    default void launchCredentialManager(@ManagePasswordsReferrer int referrer) {}

    /**
     * Retrieves a pending intent that can be used to launch the credential manager. The intent
     * is to either be used immediately or discarded.
     *
     * TODO(crbug.com/1255038): Change to abstract once downstream implements it.
     *
     * @param referrer the place that will launch the credential manager
     * @param accountName the account name that is syncing passwords.
     * @param successCallback callback called with the intent if the retrieving was successful
     * @param failureCallback callback called if the retrieving failed with the raised exception
     */
    default void getCredentialManagerLaunchIntentForAccount(@ManagePasswordsReferrer int referrer,
            String accountName, Callback<PendingIntent> successCallback,
            Callback<Exception> failureCallback) {}

    /**
     * Retrieves a pending intent that can be used to launch the credential manager. The intent
     * is to either be used immediately or discarded.
     *
     * TODO(crbug.com/1255038): Change to abstract once downstream implements it.
     *
     * @param referrer the place that will launch the credential manager
     * @param successCallback callback called with the intent if the retrieving was successful
     * @param failureCallback callback called if the retrieving failed with the raised exception
     */
    default void getCredentialManagerLaunchIntentForLocal(@ManagePasswordsReferrer int referrer,
            Callback<PendingIntent> successCallback, Callback<Exception> failureCallback) {}
}
