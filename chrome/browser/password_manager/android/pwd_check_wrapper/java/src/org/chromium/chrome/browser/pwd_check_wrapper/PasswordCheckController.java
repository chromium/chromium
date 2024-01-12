// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_check_wrapper;

import androidx.annotation.IntDef;

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
    @IntDef({PasswordStoreType.PROFILE_STORE, PasswordStoreType.ACCOUNT_STORE})
    @Retention(RetentionPolicy.SOURCE)
    @interface PasswordStoreType {
        int PROFILE_STORE = 1;
        int ACCOUNT_STORE = 2;
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

    /** Triggers the password check. */
    CompletableFuture<PasswordCheckResult> checkPasswords(@PasswordStoreType int passwordStoreType);

    /**
     * Requests the breached credentials count. It doesn't do the actual password check, only
     * returns the cached value of the previous check.
     */
    CompletableFuture<PasswordCheckResult> getBreachedCredentialsCount(
            @PasswordStoreType int passwordStoreType);

    /** Cancels pending password check and removes any registered observers. */
    void destroy();
}
