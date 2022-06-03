// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import org.chromium.base.annotations.NativeMethods;

/** Bridge, providing access to the native-side Privacy Sandbox configuration. */
public class PrivacySandboxBridge {
    public static boolean isPrivacySandboxEnabled() {
        return PrivacySandboxBridgeJni.get().isPrivacySandboxEnabled();
    }

    public static boolean isPrivacySandboxManaged() {
        return PrivacySandboxBridgeJni.get().isPrivacySandboxManaged();
    }

    public static void setPrivacySandboxEnabled(boolean enabled) {
        PrivacySandboxBridgeJni.get().setPrivacySandboxEnabled(enabled);
    }

    public static boolean isFlocEnabled() {
        return PrivacySandboxBridgeJni.get().isFlocEnabled();
    }

    public static void setFlocEnabled(boolean enabled) {
        PrivacySandboxBridgeJni.get().setFlocEnabled(enabled);
    }

    public static boolean isFlocIdResettable() {
        return PrivacySandboxBridgeJni.get().isFlocIdResettable();
    }

    public static void resetFlocId() {
        PrivacySandboxBridgeJni.get().resetFlocId();
    }

    public static String getFlocStatusString() {
        return PrivacySandboxBridgeJni.get().getFlocStatusString();
    }

    public static String getFlocGroupString() {
        return PrivacySandboxBridgeJni.get().getFlocGroupString();
    }

    public static String getFlocUpdateString() {
        return PrivacySandboxBridgeJni.get().getFlocUpdateString();
    }

    public static String getFlocDescriptionString() {
        return PrivacySandboxBridgeJni.get().getFlocDescriptionString();
    }

    public static String getFlocResetExplanationString() {
        return PrivacySandboxBridgeJni.get().getFlocResetExplanationString();
    }

    @NativeMethods
    interface Natives {
        boolean isPrivacySandboxEnabled();
        boolean isPrivacySandboxManaged();
        void setPrivacySandboxEnabled(boolean enabled);
        boolean isFlocEnabled();
        void setFlocEnabled(boolean enabled);
        boolean isFlocIdResettable();
        void resetFlocId();
        String getFlocStatusString();
        String getFlocGroupString();
        String getFlocUpdateString();
        String getFlocDescriptionString();
        String getFlocResetExplanationString();
    }
}
