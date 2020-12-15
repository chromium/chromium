// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.shell;

/** draw_fn framework side implementation for tests. */
public class DrawFn {
    public static long getDrawFnFunctionTable() {
        return nativeGetDrawFnFunctionTable();
    }

    public static void sync(int functor, boolean applyForceDark) {
        nativeSync(functor, applyForceDark);
    }

    public static void destroyReleased() {
        nativeDestroyReleased();
    }

    public static void drawGL(int functor, int width, int height, int scrollX, int scrollY) {
        nativeDrawGL(functor, width, height, scrollX, scrollY);
    }

    private static native long nativeGetDrawFnFunctionTable();
    private static native void nativeSync(int functor, boolean applyForceDark);
    private static native void nativeDestroyReleased();
    private static native void nativeDrawGL(
            int functor, int width, int height, int scrollX, int scrollY);
}
