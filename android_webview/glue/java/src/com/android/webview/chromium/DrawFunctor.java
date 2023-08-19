// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.build.annotations.UsedByReflection;

@Lifetime.Singleton
class DrawFunctor {
    public static long getDrawFnFunctionTable() {
        return nativeGetFunctionTable();
    }

    // The Android framework performs manual JNI registration on these methods, so the method
    // signatures cannot change without updating the framework. We use @UsedByReflection, while not
    // technically true, as a way to preserve these methods and their names.
    @UsedByReflection("Android framework manual registration")
    private static native long nativeGetFunctionTable();
}
