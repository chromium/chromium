// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

/**
 * The factory for creating fake {@link PasswordStoreAndroidBackend} to be used in integration
 * tests.
 */
public class FakePasswordStoreAndroidBackendFactoryImpl extends PasswordStoreAndroidBackendFactory {
    private PasswordStoreAndroidBackend mPasswordStoreAndroidBackend;

    /**
     * Returns the downstream implementation provided by subclasses.
     *
     * @return A non-null implementation of the {@link PasswordStoreAndroidBackend}.
     */
    @Override
    public PasswordStoreAndroidBackend createBackend() {
        if (mPasswordStoreAndroidBackend == null) {
            mPasswordStoreAndroidBackend = new FakePasswordStoreAndroidBackend();
        }
        return mPasswordStoreAndroidBackend;
    }
}
