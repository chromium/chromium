// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.app.PendingIntent;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Fake {@link PasswordCheckupClientHelper} to be used in integration tests. */
@NullMarked
public class FakePasswordCheckupClientHelper implements PasswordCheckupClientHelper {
    private @Nullable PendingIntent mPendingIntentForLocalCheckup;
    private @Nullable PendingIntent mPendingIntentForAccountCheckup;
    private int mBreachedCredentialsCount;
    private int mWeakCredentialsCount;
    private int mReusedCredentialsCount;
    private @Nullable Exception mError;
    private @Nullable Exception mWeakCredentialsError;

    public void setIntentForLocalCheckup(PendingIntent pendingIntent) {
        mPendingIntentForLocalCheckup = pendingIntent;
    }

    public void setIntentForAccountCheckup(PendingIntent pendingIntent) {
        mPendingIntentForAccountCheckup = pendingIntent;
    }

    public void setBreachedCredentialsCount(int count) {
        mBreachedCredentialsCount = count;
    }

    public void setWeakCredentialsCount(int count) {
        mWeakCredentialsCount = count;
    }

    public void setReusedCredentialsCount(int count) {
        mReusedCredentialsCount = count;
    }

    public void setError(Exception error) {
        mError = error;
    }

    public void setWeakCredentialsError(Exception error) {
        mWeakCredentialsError = error;
    }

    @Override
    public void getPasswordCheckupIntent(
            @PasswordCheckReferrer int referrer,
            @Nullable String accountName,
            Callback<PendingIntent> successCallback,
            Callback<Exception> failureCallback) {
        if (mError != null) {
            failureCallback.onResult(mError);
            return;
        }
        @Nullable PendingIntent intent =
                accountName == null
                        ? mPendingIntentForLocalCheckup
                        : mPendingIntentForAccountCheckup;
        assert intent != null : "intent not set";
        successCallback.onResult(intent);
    }

    @Override
    public void runPasswordCheckupInBackground(
            @PasswordCheckReferrer int referrer,
            @Nullable String accountName,
            Callback<@Nullable Void> successCallback,
            Callback<Exception> failureCallback) {
        if (mError != null) {
            failureCallback.onResult(mError);
            return;
        }
        successCallback.onResult(null);
    }

    @Override
    public void getBreachedCredentialsCount(
            @PasswordCheckReferrer int referrer,
            @Nullable String accountName,
            Callback<Integer> successCallback,
            Callback<Exception> failureCallback) {
        if (mError != null) {
            failureCallback.onResult(mError);
            return;
        }
        successCallback.onResult(mBreachedCredentialsCount);
    }

    @Override
    public void getWeakCredentialsCount(
            @PasswordCheckReferrer int referrer,
            @Nullable String accountName,
            Callback<Integer> successCallback,
            Callback<Exception> failureCallback) {
        if (mError != null) {
            failureCallback.onResult(mError);
            return;
        }

        if (mWeakCredentialsError != null) {
            failureCallback.onResult(mWeakCredentialsError);
            return;
        }

        successCallback.onResult(mWeakCredentialsCount);
    }

    @Override
    public void getReusedCredentialsCount(
            @PasswordCheckReferrer int referrer,
            @Nullable String accountName,
            Callback<Integer> successCallback,
            Callback<Exception> failureCallback) {
        if (mError != null) {
            failureCallback.onResult(mError);
            return;
        }
        successCallback.onResult(mReusedCredentialsCount);
    }
}
