// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth;

/**
 * Fake implementation of the Android Credential Manager PrepareGetCredentialResponse object.
 */
public final class FakeAndroidCredManPrepareGetCredentialResponse {
    public boolean hasAuthenticationResults() {
        return false;
    }

    public boolean hasCredentialResults(String credentialType) {
        return true;
    }

    public boolean hasRemoteResults() {
        return true;
    }

    public Object getPendingGetCredentialHandle() {
        return new Object();
    }
}
