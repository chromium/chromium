// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.app.PendingIntent;

import org.chromium.base.Callback;

/**
 * Fake {@link CredentialManagerLauncher} to be used in integration tests.
 */
public class FakeCredentialManagerLauncher implements CredentialManagerLauncher {
    private PendingIntent mPendingIntent;
    private Integer mError;

    public void setIntent(PendingIntent pendingIntent) {
        mPendingIntent = pendingIntent;
    }
    public void setCredentialManagerError(Integer error) {
        mError = error;
    }

    @Override
    public void getCredentialManagerIntentForAccount(@ManagePasswordsReferrer int referrer,
            String accountName, Callback<PendingIntent> successCallback,
            Callback<Integer> failureCallback) {
        if (accountName == null) {
            failureCallback.onResult(CredentialManagerError.NO_ACCOUNT_NAME);
            return;
        }
        getCredentialManagerLaunchIntent(successCallback, failureCallback);
    }

    @Override
    public void getCredentialManagerIntentForLocal(@ManagePasswordsReferrer int referrer,
            Callback<PendingIntent> successCallback, Callback<Integer> failureCallback) {
        getCredentialManagerLaunchIntent(successCallback, failureCallback);
    }

    private void getCredentialManagerLaunchIntent(
            Callback<PendingIntent> successCallback, Callback<Integer> failureCallback) {
        if (mError != null) {
            failureCallback.onResult(mError);
            return;
        }
        successCallback.onResult(mPendingIntent);
    }
}
