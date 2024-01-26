// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_check_wrapper;

import org.chromium.chrome.browser.password_check.PasswordCheck;
import org.chromium.chrome.browser.password_check.PasswordCheckFactory;
import org.chromium.chrome.browser.password_check.PasswordCheckUIStatus;
import org.chromium.components.browser_ui.settings.SettingsLauncher;

import java.util.concurrent.CompletableFuture;

class ChromeNativePasswordCheckController
        implements PasswordCheckController, PasswordCheck.Observer {
    private CompletableFuture<Integer> mPasswordsTotalCount;
    private CompletableFuture<PasswordCheckResult> mPasswordCheckResult;
    private final PasswordCheck mPasswordCheck;

    public ChromeNativePasswordCheckController(SettingsLauncher settingsLauncher) {
        mPasswordCheck = PasswordCheckFactory.getOrCreate(settingsLauncher);
    }

    @Override
    public CompletableFuture<PasswordCheckResult> checkPasswords(
            @PasswordStorageType int passwordStorageType) {
        mPasswordCheckResult = new CompletableFuture<>();
        mPasswordsTotalCount = new CompletableFuture<>();
        // Start observing the password check events (including data loads).
        mPasswordCheck.addObserver(this, false);
        mPasswordCheck.startCheck();
        return mPasswordCheckResult;
    }

    @Override
    public void destroy() {
        mPasswordCheck.stopCheck();
        mPasswordCheck.removeObserver(this);
    }

    @Override
    public CompletableFuture<PasswordCheckResult> getBreachedCredentialsCount(
            int passwordStorageType) {
        mPasswordCheckResult = new CompletableFuture<>();
        mPasswordsTotalCount = new CompletableFuture<>();
        mPasswordCheck.addObserver(this, true);
        return mPasswordCheckResult;
    }

    // PasswordCheck.Observer implementation.
    @Override
    public void onCompromisedCredentialsFetchCompleted() {
        mPasswordsTotalCount.thenAccept(
                totalCount -> {
                    int breachedCount = mPasswordCheck.getCompromisedCredentialsCount();
                    mPasswordCheckResult.complete(
                            new PasswordCheckResult(totalCount, breachedCount));
                    mPasswordCheck.removeObserver(this);
                });
    }

    @Override
    public void onSavedPasswordsFetchCompleted() {
        int totalCount = mPasswordCheck.getSavedPasswordsCount();
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
            int totalCount = mPasswordCheck.getSavedPasswordsCount();
            int breachedCount = mPasswordCheck.getCompromisedCredentialsCount();
            mPasswordCheckResult.complete(new PasswordCheckResult(totalCount, breachedCount));
        }

        mPasswordCheck.removeObserver(this);
    }

    /** Not relevant for this controller. */
    @Override
    public void onPasswordCheckProgressChanged(int alreadyProcessed, int remainingInQueue) {}
    // End of PasswordCheck.Observer implementation.
}
