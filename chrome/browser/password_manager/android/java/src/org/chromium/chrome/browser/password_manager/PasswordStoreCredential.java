// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import org.jni_zero.CalledByNative;

import org.chromium.url.GURL;

/** This class represents key elements of stored credential in the password store. */
public class PasswordStoreCredential {
    private final GURL mUrl;
    private final String mUsername;
    private final String mPassword;

    /**
     * Constructs an instance of PasswordStoreCredential. Arguments should not be null.
     *
     * @param url The associated URL to this credential.
     * @param username The username used to identify this credential.
     * @param password The password associated to this credential.
     */
    public PasswordStoreCredential(GURL url, String username, String password) {
        assert url != null;
        assert username != null;
        assert password != null;
        mUrl = url;
        mUsername = username;
        mPassword = password;
    }

    @CalledByNative
    public GURL getUrl() {
        return mUrl;
    }

    @CalledByNative
    public String getUsername() {
        return mUsername;
    }

    @CalledByNative
    public String getPassword() {
        return mPassword;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;
        PasswordStoreCredential that = (PasswordStoreCredential) o;
        return mUrl.equals(that.mUrl)
                && mUsername.equals(that.mUsername)
                && mPassword.equals(that.mPassword);
    }

    @Override
    public String toString() {
        return "PasswordStoreCredential{"
                + "url="
                + mUrl.getSpec()
                + ", username="
                + mUsername
                + ", password="
                + mPassword
                + '}';
    }
}
