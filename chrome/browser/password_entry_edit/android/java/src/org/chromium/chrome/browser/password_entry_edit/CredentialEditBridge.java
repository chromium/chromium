// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_entry_edit;

import android.content.Context;

import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.browser_ui.settings.SettingsLauncher;

/**
 * Class mediating the communication between the credential edit UI and the C++ part responsible
 * for saving the changes.
 */
class CredentialEditBridge {
    private static CredentialEditBridge sCredentialEditBridge;

    private long mNativeCredentialEditBridge;

    private CredentialEditBridge() {}

    @CalledByNative
    static @Nullable CredentialEditBridge maybeCreate() {
        // There can only be one bridge at a time and it shouldn't be shared.
        if (sCredentialEditBridge != null) return null;
        sCredentialEditBridge = new CredentialEditBridge();
        return sCredentialEditBridge;
    }

    @CalledByNative
    void initAndLaunchUi(long nativeCredentialEditBridge, Context context,
            SettingsLauncher settingsLauncher, boolean isBlockedCredential,
            boolean isFederatedCredential) {
        mNativeCredentialEditBridge = nativeCredentialEditBridge;
        // TODO
    }

    @CalledByNative
    void setCredential(String displayUrlOrAppName, String username, String password,
            String displayFederationOrigin, boolean isInsecureCredential) {
    }

    @CalledByNative
    void setExistingUsernames(String[] existingUsernames) {

    }

    @CalledByNative
    void destroy() {
        mNativeCredentialEditBridge = 0;
        sCredentialEditBridge = null;
    }

    @NativeMethods
    interface Natives {
        void getCredential(long nativeCredentialEditBridge);
        void getExistingUsernames(long nativeCredentialEditBridge);
        void saveChanges(long nativeCredentialEditBridge, String username, String password);
        void deleteCredential(long nativeCredentialEditBridge);
        void onUIDismissed(long nativeCredentialEditBridge);
    }
}
