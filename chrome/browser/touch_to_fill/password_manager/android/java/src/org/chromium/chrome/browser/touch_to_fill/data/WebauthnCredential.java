// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.data;

import org.jni_zero.CalledByNative;

/**
 * This class holds the data used to represent a selectable Web Authentication credential in the
 * Touch To Fill sheet.
 */
public class WebauthnCredential {
    private final String mRpId;
    private final byte[] mCredentialId;
    private final byte[] mUserId;
    private final String mUsername;

    /**
     * @param rpId Relying party identifier
     * @param credentialId Unique identifier for the credential
     * @param userId User handle
     * @param username Username shown to the user.
     */
    public WebauthnCredential(String rpId, byte[] credentialId, byte[] userId, String username) {
        mRpId = rpId;
        mCredentialId = credentialId;
        mUserId = userId;
        mUsername = username;
    }

    @CalledByNative
    public String getRpId() {
        return mRpId;
    }

    @CalledByNative
    public byte[] getCredentialId() {
        return mCredentialId;
    }

    @CalledByNative
    public byte[] getUserId() {
        return mUserId;
    }

    @CalledByNative
    public String getUsername() {
        return mUsername;
    }
}
