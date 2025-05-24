// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet;

import org.chromium.build.annotations.NullMarked;

/** This class holds the data used to pass a fill request. */
@NullMarked
class CredentialFillRequest {
    private final Credential mCredential;
    private final boolean mRequestsToFillPassword;

    CredentialFillRequest(Credential credential, boolean requestsToFillPassword) {
        mCredential = credential;
        mRequestsToFillPassword = requestsToFillPassword;
    }

    Credential getCredential() {
        return mCredential;
    }

    boolean getRequestsToFillPassword() {
        return mRequestsToFillPassword;
    }
}
