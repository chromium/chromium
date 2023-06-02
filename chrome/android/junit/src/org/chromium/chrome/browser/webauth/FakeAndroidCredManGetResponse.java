// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth;

/**
 * Fake implementation of the Android Credential Manager GetCredentialResponse object.
 */
public final class FakeAndroidCredManGetResponse {
    private final FakeAndroidCredential mCredential;

    public FakeAndroidCredManGetResponse(FakeAndroidCredential credential) {
        mCredential = credential;
    }

    public FakeAndroidCredential getCredential() {
        return mCredential;
    }
}
