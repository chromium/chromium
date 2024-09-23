// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.app.PendingIntent;

import org.chromium.base.Callback;

/**
 * The factory for creating a fake {@link CredentialManagerLauncher} to be used in integration
 * tests.
 */
public class FakeCredentialManagerLauncherFactoryImpl extends CredentialManagerLauncherFactory {
    private FakeCredentialManagerLauncher mLauncher;
    private PendingIntent mPendingIntent;
    private Callback<PendingIntent> mSuccessCallback;
    private Callback<Exception> mFailureCallback;

    public void setSuccessCallback(Callback<PendingIntent> successCallback) {
        mSuccessCallback = successCallback;
    }

    public void setFailureCallback(Callback<Exception> failureCallback) {
        mFailureCallback = failureCallback;
    }

    public void setIntent(PendingIntent pendingIntent) {
        mPendingIntent = pendingIntent;
    }

    /** Returns the fake implementation of {@link CredentialManagerLauncher} used for tests. */
    @Override
    public CredentialManagerLauncher createLauncher() {
        if (mLauncher == null) {
            mLauncher = new FakeCredentialManagerLauncher();
            mLauncher.setSuccessCallback(mSuccessCallback);
            mLauncher.setFailureCallback(mFailureCallback);
            mLauncher.setIntent(mPendingIntent);
        }
        return mLauncher;
    }
}
