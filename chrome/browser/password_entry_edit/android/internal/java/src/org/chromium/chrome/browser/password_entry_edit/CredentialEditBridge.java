// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.password_entry_edit;

import org.chromium.base.annotations.CalledByNative;

/**
 * Class mediating the communication between the credential edit UI and the C++ part responsible
 * for saving the changes.
 */
public class CredentialEditBridge {
    private long mNativeCredentialEditBridge;

    private CredentialEditBridge(long nativeCredentialEditBridge) {
        mNativeCredentialEditBridge = nativeCredentialEditBridge;
    }

    @CalledByNative
    public static CredentialEditBridge create(long nativeCredentialEditBridge) {
        return new CredentialEditBridge(nativeCredentialEditBridge);
    }

    @CalledByNative
    public void destroy() {
        mNativeCredentialEditBridge = 0;
    }
}
