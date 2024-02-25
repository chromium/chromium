// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import org.chromium.base.ResettersForTesting;

/** Helper class to check PasswordManager backend availability. */
public abstract class PasswordManagerBackendSupportHelper {
    private static PasswordManagerBackendSupportHelper sInstance;

    /**
     * Return an instance of PasswordManagerBackendSupportHelper. If no helper was used yet, it is
     * created.
     */
    public static PasswordManagerBackendSupportHelper getInstance() {
        if (sInstance == null) sInstance = new PasswordManagerBackendSupportHelperImpl();
        return sInstance;
    }

    /**
     * Returns whether the downstream implementation is present.
     * Existing implementation may require an update before it could be used.
     *
     * @return True if backend is present, false otherwise.
     */
    public boolean isBackendPresent() {
        return false;
    }

    /**
     * Returns whether the GMS Core version is not supported and needs to be updated.
     *
     * @return True if update is needed, false otherwise.
     */
    public boolean isUpdateNeeded() {
        return false;
    }

    public static void setInstanceForTesting(
            PasswordManagerBackendSupportHelper backendSupportHelper) {
        var oldValue = sInstance;
        sInstance = backendSupportHelper;
        ResettersForTesting.register(() -> sInstance = oldValue);
    }
}
