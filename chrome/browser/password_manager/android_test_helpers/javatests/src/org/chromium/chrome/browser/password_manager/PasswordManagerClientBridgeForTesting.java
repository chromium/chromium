// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.WebContents;

/**
 * Android bridge to |ChromePasswordManagerClient| for Java tests.
 */
@JNINamespace("password_manager")
public class PasswordManagerClientBridgeForTesting {
    @VisibleForTesting
    public static void setLeakDialogWasShownForTesting(WebContents webContents, boolean value) {
        PasswordManagerClientBridgeForTestingJni.get().setLeakDialogWasShownForTesting(
                webContents, value);
    }

    @NativeMethods
    interface Natives {
        void setLeakDialogWasShownForTesting(WebContents webContents, boolean value);
    }
}
