// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import org.chromium.base.annotations.NativeMethods;

/** Bridge, providing access to the native-side Privacy Sandbox configuration. */
public class PrivacySandboxBridge {
    public static boolean isPrivacySandboxSettingsFunctional() {
        return PrivacySandboxBridgeJni.get().isPrivacySandboxSettingsFunctional();
    }

    public static boolean isPrivacySandboxEnabled() {
        return PrivacySandboxBridgeJni.get().isPrivacySandboxEnabled();
    }

    public static boolean isPrivacySandboxManaged() {
        return PrivacySandboxBridgeJni.get().isPrivacySandboxManaged();
    }

    public static void setPrivacySandboxEnabled(boolean enabled) {
        PrivacySandboxBridgeJni.get().setPrivacySandboxEnabled(enabled);
    }

    @NativeMethods
    interface Natives {
        boolean isPrivacySandboxSettingsFunctional();
        boolean isPrivacySandboxEnabled();
        boolean isPrivacySandboxManaged();
        void setPrivacySandboxEnabled(boolean enabled);
    }
}
