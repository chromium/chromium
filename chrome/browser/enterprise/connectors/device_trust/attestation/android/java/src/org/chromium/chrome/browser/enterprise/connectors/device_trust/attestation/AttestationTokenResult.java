// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.enterprise.connectors.device_trust.attestation;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

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

    @CalledByNative
    @JniType("std::optional<std::vector<uint8_t>>")
    public byte @Nullable [] getToken() {
        return mToken;
    }

    @CalledByNative
    @JniType("std::optional<std::string>")
    public @Nullable String getErrorMessage() {
        return mErrorMessage;
    }
}
