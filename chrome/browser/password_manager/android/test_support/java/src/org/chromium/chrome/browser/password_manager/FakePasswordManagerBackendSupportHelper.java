// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

/** Fake {@link PasswordManagerBackendSupportHelper} to be used in integration tests. */
public class FakePasswordManagerBackendSupportHelper extends PasswordManagerBackendSupportHelper {
    private static FakePasswordManagerBackendSupportHelper sInstance;

    private boolean mBackendPresent;
    private boolean mUpdateNeeded;

    @Override
    public boolean isBackendPresent() {
        return mBackendPresent;
    }

    @Override
    public boolean isUpdateNeeded() {
        return mUpdateNeeded;
    }

    public void setBackendPresent(boolean backendPresent) {
        mBackendPresent = backendPresent;
    }

    public void setUpdateNeeded(boolean updateNeeded) {
        mUpdateNeeded = updateNeeded;
    }
}
