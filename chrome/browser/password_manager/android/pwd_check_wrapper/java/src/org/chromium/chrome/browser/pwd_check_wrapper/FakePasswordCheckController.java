// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_check_wrapper;

import java.util.concurrent.CompletableFuture;

public class FakePasswordCheckController implements PasswordCheckController {
    private CompletableFuture<PasswordCheckResult> mPasswordCheckResult;

    public void setPasswordCheckResult(PasswordCheckResult result) {
        mPasswordCheckResult.complete(result);
    }

    @Override
    public CompletableFuture<PasswordCheckResult> checkPasswords(int passwordStoreType) {
        mPasswordCheckResult = new CompletableFuture<>();
        return mPasswordCheckResult;
    }

    @Override
    public CompletableFuture<PasswordCheckResult> getBreachedCredentialsCount(
            int passwordStoreType) {
        mPasswordCheckResult = new CompletableFuture<>();
        return mPasswordCheckResult;
    }

    @Override
    public void destroy() {}
}
