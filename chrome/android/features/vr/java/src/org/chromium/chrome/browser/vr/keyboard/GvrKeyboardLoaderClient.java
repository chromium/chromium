// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.keyboard;

import android.content.Context;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/** Loads the GVR keyboard SDK dynamically using the Keyboard Service. */
@JNINamespace("vr")
public class GvrKeyboardLoaderClient {

    @CalledByNative
    public static long loadKeyboardSDK() {
    }

    @CalledByNative
    public static void closeKeyboardSDK(long handle) {
    }

    @CalledByNative
    public static Context getContextWrapper() {
        return null;
    }

    @CalledByNative
    public static Object getRemoteClassLoader() {
        return null;
    }

}
