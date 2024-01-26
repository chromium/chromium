// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_check_wrapper;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.password_manager.PasswordManagerHelper;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.sync.SyncService;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.OptionalInt;
import java.util.concurrent.CompletableFuture;

/**
 * The controller, which manages the breached credentials check. It runs the password check
 * asynchronously and provides the result whenever it's ready.
 */
public interface PasswordCheckController {

    /**
     * PROFILE_STORE is the store associated with local non-syncing passwords in GMS core.
     * ACCOUNT_STORE is the store associated with the user's account set up on the phone.
     */
    @IntDef({PasswordStorageType.LOCAL_STORAGE, PasswordStorageType.ACCOUNT_STORAGE})
    @Retention(RetentionPolicy.SOURCE)
    @interface PasswordStorageType {
        int LOCAL_STORAGE = 1;
        int ACCOUNT_STORAGE = 2;
    }

    public static class PasswordCheckResult {
        private OptionalInt mTotalPasswordsCount = OptionalInt.empty();
        private OptionalInt mBreachedCount = OptionalInt.empty();
        private Exception mError;

        public PasswordCheckResult(int totalPasswordsCount, int breachedCount) {
            mTotalPasswordsCount = OptionalInt.of(totalPasswordsCount);
            mBreachedCount = OptionalInt.of(breachedCount);
        }

        public PasswordCheckResult(Exception error) {
            mError = error;
        }

        public OptionalInt getBreachedCount() {
            return mBreachedCount;
        }

        public OptionalInt getTotalPasswordsCount() {
            return mTotalPasswordsCount;
        }

        public Exception getError() {
            return mError;
        }
    }

    static String getAccountNameForPasswordStorageType(
            @PasswordStorageType int passwordStorageType, SyncService syncService) {
        switch (passwordStorageType) {
            case PasswordStorageType.LOCAL_STORAGE:
                return null;
            case PasswordStorageType.ACCOUNT_STORAGE:
                assert PasswordManagerHelper.hasChosenToSyncPasswords(syncService)
                        : "The account storage is only available if password sync is on.";
                return CoreAccountInfo.getEmailFrom(syncService.getAccountInfo());
        }
        assert false : "Unknown PasswordStorageType: " + passwordStorageType;
        return null;
    }

    /** Triggers the password check. */
    CompletableFuture<PasswordCheckResult> checkPasswords(
            @PasswordStorageType int passwordStorageType);

    /**
     * Requests the breached credentials count. It doesn't do the actual password check, only
     * returns the cached value of the previous check.
     */
    CompletableFuture<PasswordCheckResult> getBreachedCredentialsCount(
            @PasswordStorageType int passwordStorageType);

    /** Cancels pending password check and removes any registered observers. */
    void destroy();
}
