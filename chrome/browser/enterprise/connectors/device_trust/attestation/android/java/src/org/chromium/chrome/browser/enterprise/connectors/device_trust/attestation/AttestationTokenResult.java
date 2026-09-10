// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.enterprise.connectors.device_trust.attestation;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Holds the result of an attestation token generation request. */
@NullMarked
public class AttestationTokenResult {
    private final byte @Nullable [] mToken;
    private final @Nullable String mErrorMessage;

    public AttestationTokenResult(byte @Nullable [] token, @Nullable String errorMessage) {
        mToken = token;
        mErrorMessage = errorMessage;
    }

    public byte @Nullable [] getToken() {
        return mToken;
    }

    public @Nullable String getErrorMessage() {
        return mErrorMessage;
    }
}
