// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_check_wrapper;

import java.util.HashMap;
import java.util.concurrent.CompletableFuture;

public class FakePasswordCheckController implements PasswordCheckController {
    private HashMap<Integer, CompletableFuture<PasswordCheckResult>> mPasswordCheckResults =
            new HashMap<>();

    public void setPasswordCheckResult(
            @PasswordStorageType int passwordStorageType, PasswordCheckResult result) {
        mPasswordCheckResults.get(passwordStorageType).complete(result);
    }

    public CompletableFuture<PasswordCheckResult> getFuturePasswordCheckResultForStorageType(
            @PasswordStorageType int passwordStorageType) {
        return mPasswordCheckResults.get(passwordStorageType);
    }

    @Override
    public CompletableFuture<PasswordCheckResult> checkPasswords(
            @PasswordStorageType int passwordStoreType) {
        mPasswordCheckResults.put(passwordStoreType, new CompletableFuture<>());
        return mPasswordCheckResults.get(passwordStoreType);
    }

    @Override
    public CompletableFuture<PasswordCheckResult> getBreachedCredentialsCount(
            @PasswordStorageType int passwordStoreType) {
        mPasswordCheckResults.put(passwordStoreType, new CompletableFuture<>());
        return mPasswordCheckResults.get(passwordStoreType);
    }

    @Override
    public void destroy() {}
}
