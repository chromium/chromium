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

    @Nullable
    static CredentialEditBridge get() {
        return sCredentialEditBridge;
    }

    private CredentialEditBridge() {}

    @CalledByNative
    static @Nullable CredentialEditBridge maybeCreate() {
        // There can only be one bridge at a time and it shouldn't be shared.
        if (sCredentialEditBridge != null) return null;
        sCredentialEditBridge = new CredentialEditBridge();
        return sCredentialEditBridge;
    }

    @CalledByNative
    void initAndLaunchUi(
            long nativeCredentialEditBridge, Context context, SettingsLauncher settingsLauncher) {
        mNativeCredentialEditBridge = nativeCredentialEditBridge;
        settingsLauncher.launchSettingsActivity(context, CredentialEditFragmentView.class);
    }

    @CalledByNative
    void setCredential(String displayUrlOrAppName, String username, String password,
            String displayFederationOrigin) {
        // TODO(crbug.com/1170289): Pass the credential data to the UI to be displayed.
    }

    // This can be called either before or after the native counterpart has gone away, depending
    // on where the edit component is being destroyed from.
    void onUiDismissed() {
        if (mNativeCredentialEditBridge != 0) {
            CredentialEditBridgeJni.get().onUIDismissed(mNativeCredentialEditBridge);
        }
        mNativeCredentialEditBridge = 0;
        sCredentialEditBridge = null;
    }

    @CalledByNative
    void destroy() {
        // TODO(crbug.com/1175785): Dismiss the UI, if it wasn't dismissed already.
        mNativeCredentialEditBridge = 0;
        sCredentialEditBridge = null;
    }

    @NativeMethods
    interface Natives {
        void getCredential(long nativeCredentialEditBridge);
        void onUIDismissed(long nativeCredentialEditBridge);
    }
}
