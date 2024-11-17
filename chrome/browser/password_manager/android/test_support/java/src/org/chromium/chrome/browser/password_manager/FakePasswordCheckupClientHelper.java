// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.app.PendingIntent;

import org.chromium.base.Callback;

import java.util.Optional;

/** Fake {@link PasswordCheckupClientHelper} to be used in integration tests. */
public class FakePasswordCheckupClientHelper implements PasswordCheckupClientHelper {
    private PendingIntent mPendingIntentForLocalCheckup;
    private PendingIntent mPendingIntentForAccountCheckup;
    private Integer mBreachedCredentialsCount = 0;
    private Integer mWeakCredentialsCount = 0;
    private Integer mReusedCredentialsCount = 0;
    private Exception mError;
    private Exception mWeakCredentialsError;

    public void setIntentForLocalCheckup(PendingIntent pendingIntent) {
        mPendingIntentForLocalCheckup = pendingIntent;
    }

    public void setIntentForAccountCheckup(PendingIntent pendingIntent) {
        mPendingIntentForAccountCheckup = pendingIntent;
    }

    public void setBreachedCredentialsCount(Integer count) {
        mBreachedCredentialsCount = count;
    }

    public void setWeakCredentialsCount(Integer count) {
        mWeakCredentialsCount = count;
    }

    public void setReusedCredentialsCount(Integer count) {
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
            Optional<String> accountName,
            Callback<PendingIntent> successCallback,
            Callback<Exception> failureCallback) {
        if (mError != null) {
            failureCallback.onResult(mError);
            return;
        }
        successCallback.onResult(
                accountName.isPresent()
                        ? mPendingIntentForAccountCheckup
                        : mPendingIntentForLocalCheckup);
    }

    @Override
    public void getPasswordCheckupIntent(
            @PasswordCheckReferrer int referrer,
            String accountName,
            Callback<PendingIntent> successCallback,
            Callback<Exception> failureCallback) {
        if (mError != null) {
            failureCallback.onResult(mError);
            return;
        }
        successCallback.onResult(
                accountName.isEmpty()
                        ? mPendingIntentForLocalCheckup
                        : mPendingIntentForAccountCheckup);
    }

    @Override
    public void runPasswordCheckupInBackground(
            @PasswordCheckReferrer int referrer,
            Optional<String> accountName,
            Callback<Void> successCallback,
            Callback<Exception> failureCallback) {
        if (mError != null) {
            failureCallback.onResult(mError);
            return;
        }
        successCallback.onResult(null);
    }

    @Override
    public void runPasswordCheckupInBackground(
            @PasswordCheckReferrer int referrer,
            String accountName,
            Callback<Void> successCallback,
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
            Optional<String> accountName,
            Callback<Integer> successCallback,
            Callback<Exception> failureCallback) {
        if (mError != null) {
            failureCallback.onResult(mError);
            return;
        }
        successCallback.onResult(mBreachedCredentialsCount);
    }

    @Override
    public void getBreachedCredentialsCount(
            @PasswordCheckReferrer int referrer,
            String accountName,
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
            String accountName,
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
            String accountName,
            Callback<Integer> successCallback,
            Callback<Exception> failureCallback) {
        if (mError != null) {
            failureCallback.onResult(mError);
            return;
        }
        successCallback.onResult(mReusedCredentialsCount);
    }
}
