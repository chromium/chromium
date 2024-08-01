// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_check_wrapper;

import org.chromium.chrome.browser.password_check.PasswordCheck;
import org.chromium.chrome.browser.password_check.PasswordCheckFactory;
import org.chromium.chrome.browser.password_check.PasswordCheckUIStatus;

import java.util.concurrent.CompletableFuture;

class ChromeNativePasswordCheckController
        implements PasswordCheckController, PasswordCheck.Observer {
    private CompletableFuture<Integer> mPasswordsTotalCount;
    private CompletableFuture<PasswordCheckResult> mPasswordCheckResult;

    @Override
    public CompletableFuture<PasswordCheckResult> checkPasswords(
            @PasswordStorageType int passwordStorageType) {
        mPasswordCheckResult = new CompletableFuture<>();
        mPasswordsTotalCount = new CompletableFuture<>();
        // Start observing the password check events (including data loads).
        getPasswordCheck().addObserver(this, false);
        getPasswordCheck().startCheck();
        return mPasswordCheckResult;
    }

    @Override
    public void destroy() {
        PasswordCheck passwordCheck = PasswordCheckFactory.getPasswordCheckInstance();
        if (passwordCheck == null) return;

        passwordCheck.stopCheck();
        passwordCheck.removeObserver(this);
    }

    @Override
    public CompletableFuture<PasswordCheckResult> getBreachedCredentialsCount(
            int passwordStorageType) {
        mPasswordCheckResult = new CompletableFuture<>();

        // Check if the user is signed out.
        if (!getPasswordCheck().hasAccountForRequest()) {
            PasswordCheckNativeException error =
                    new PasswordCheckNativeException(
                            "The user is signed out of their account.",
                            PasswordCheckUIStatus.ERROR_SIGNED_OUT);
            mPasswordCheckResult.complete(new PasswordCheckResult(error));
            return mPasswordCheckResult;
        }

        mPasswordsTotalCount = new CompletableFuture<>();
        getPasswordCheck().addObserver(this, true);
        return mPasswordCheckResult;
    }

    private PasswordCheck getPasswordCheck() {
        PasswordCheck passwordCheck = PasswordCheckFactory.getOrCreate();
        assert passwordCheck != null : "Password Check UI component needs native counterpart!";
        return passwordCheck;
    }

    // PasswordCheck.Observer implementation.
    @Override
    public void onCompromisedCredentialsFetchCompleted() {
        mPasswordsTotalCount.thenAccept(
                totalCount -> {
                    int breachedCount = getPasswordCheck().getCompromisedCredentialsCount();
                    mPasswordCheckResult.complete(
                            new PasswordCheckResult(totalCount, breachedCount));
                    getPasswordCheck().removeObserver(this);
                });
    }

    @Override
    public void onSavedPasswordsFetchCompleted() {
        int totalCount = getPasswordCheck().getSavedPasswordsCount();
        mPasswordsTotalCount.complete(totalCount);
    }

    @Override
    public void onPasswordCheckStatusChanged(@PasswordCheckUIStatus int status) {
        if (status == PasswordCheckUIStatus.RUNNING) {
            return;
        }

        // Handle error state.
        if (status != PasswordCheckUIStatus.IDLE) {
            PasswordCheckNativeException error =
                    new PasswordCheckNativeException(
                            "Password check finished with the error " + status + ".", status);
            mPasswordCheckResult.complete(new PasswordCheckResult(error));
        } else {
            int totalCount = getPasswordCheck().getSavedPasswordsCount();
            int breachedCount = getPasswordCheck().getCompromisedCredentialsCount();
            mPasswordCheckResult.complete(new PasswordCheckResult(totalCount, breachedCount));
        }

        getPasswordCheck().removeObserver(this);
    }

    /** Not relevant for this controller. */
    @Override
    public void onPasswordCheckProgressChanged(int alreadyProcessed, int remainingInQueue) {}
    // End of PasswordCheck.Observer implementation.
}
