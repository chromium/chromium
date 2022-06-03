// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Android bridge to |PasswordScriptsFetcher|.
 */
@JNINamespace("password_manager")
public class PasswordScriptsFetcherBridge {
    public static void prewarmCache() {
        PasswordScriptsFetcherBridgeJni.get().prewarmCache();
    }

    @NativeMethods
    interface Natives {
        void prewarmCache();
    }
}
