// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import org.chromium.base.annotations.JniIgnoreNatives;

@JniIgnoreNatives
class DrawFunctor {
    public static long getDrawFnFunctionTable() {
        return nativeGetFunctionTable();
    }

    // The Android framework performs manual JNI registration on this method,
    // so the method signature cannot change without updating the framework.
    private static native long nativeGetFunctionTable();
}
