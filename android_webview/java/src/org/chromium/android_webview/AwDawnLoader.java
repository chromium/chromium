// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;

/** Native bridge for exposing Dawn's WebGPU proc table from WebView. */
@JNINamespace("android_webview")
@NullMarked
public final class AwDawnLoader {
    public static final long WEBGPU_ERROR_APP_TOO_OLD = -1L;
    public static final long WEBGPU_ERROR_WEBVIEW_TOO_OLD = 0L;

    public static long getWGPUProcGetProcAddress(int majorVersion, long minorVersion) {
        return AwDawnLoaderJni.get().getWGPUProcGetProcAddress(majorVersion, minorVersion);
    }

    private AwDawnLoader() {}

    @NativeMethods
    interface Natives {
        long getWGPUProcGetProcAddress(int majorVersion, long minorVersion);
    }
}
