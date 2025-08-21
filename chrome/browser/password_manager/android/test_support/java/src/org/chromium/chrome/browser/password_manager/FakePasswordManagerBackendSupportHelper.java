// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

/** Fake {@link PasswordManagerBackendSupportHelper} to be used in integration tests. */
public class FakePasswordManagerBackendSupportHelper extends PasswordManagerBackendSupportHelper {
    private boolean mBackendPresent;

    @Override
    public boolean isBackendPresent() {
        return mBackendPresent;
    }

    public void setBackendPresent(boolean backendPresent) {
        mBackendPresent = backendPresent;
    }
}
