// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common.crash;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

/** A helper class for WebView-specific handling of Java crashes. */
@JNINamespace("android_webview")
public class AwCrashReporterClient {
    // The filename prefix used by Chromium proguarding, which we use to
    // recognise stack frames that reference WebView.
    private static final String CHROMIUM_PREFIX = "chromium-";

    /**
     * Determine if a Throwable should be reported to the crash reporting mechanism.
     *
     * We report exceptions if any stack frame corresponds to a class directly defined in the
     * WebView classloader (which may have been proguarded) or is defined in an ancestral
     * classloader, but has package android.webkit. (i.e. it is a framework WebView class).
     * Technically we should also include androidx.webkit classes, but these are defined in the app
     * classloader, and may have been proguarded.
     *
     * @param t The throwable to check.
     * @return True if any frame of the stack trace matches the above rule.
     */
    @VisibleForTesting
    @CalledByNative
    public static boolean stackTraceContainsWebViewCode(Throwable t) {
        for (StackTraceElement frame : t.getStackTrace()) {
            if (frame.getClassName().startsWith("android.webkit.")
                    || frame.getClassName().startsWith("com.android.webview.")
                    || frame.getClassName().startsWith("org.chromium.")
                    || (frame.getFileName() != null
                            && frame.getFileName().startsWith(CHROMIUM_PREFIX))) {
                return true;
            }
        }
        return false;
    }
}
