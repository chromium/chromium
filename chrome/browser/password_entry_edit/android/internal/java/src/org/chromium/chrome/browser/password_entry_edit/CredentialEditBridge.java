// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_entry_edit;

import android.content.Context;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.browser_ui.settings.SettingsLauncher;

/**
 * Class mediating the communication between the credential edit UI and the C++ part responsible
 * for saving the changes.
 */
class CredentialEditBridge {
    private long mNativeCredentialEditBridge;

    private CredentialEditBridge(
            long nativeCredentialEditBridge, Context context, SettingsLauncher settingsLauncher) {
        mNativeCredentialEditBridge = nativeCredentialEditBridge;
        settingsLauncher.launchSettingsActivity(context, CredentialEditFragmentView.class);
    }

    @CalledByNative
    static CredentialEditBridge create(
            long nativeCredentialEditBridge, Context context, SettingsLauncher settingsLauncher) {
        return new CredentialEditBridge(nativeCredentialEditBridge, context, settingsLauncher);
    }

    @CalledByNative
    void setCredential(String displayUrlOrAppName, String username, String password,
            String displayFederationOrigin) {
        // TODO(crbug.com/1170289): Pass the credential data to the UI to be displayed.
    }

    @CalledByNative
    void destroy() {
        mNativeCredentialEditBridge = 0;
    }

    @NativeMethods
    interface Natives {
        void getCredential(long nativeCredentialEditBridge);
    }
}
