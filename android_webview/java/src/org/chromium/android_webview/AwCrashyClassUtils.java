// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.common.AwSwitches;
import org.chromium.base.CommandLine;

/** A helper class for testing related to Java and Native crashes. */
@JNINamespace("android_webview")
public final class AwCrashyClassUtils {

    public static void maybeCrashIfEnabled() {
        if (shouldCrashJava()) {
            throw new RuntimeException("WebView Forced Java Crash for WebView Browser Process");
        } else if (shouldCrashNative()) {
            AwCrashyClassUtilsJni.get().crashInNative();
        }
    }

    @VisibleForTesting
    public static boolean shouldCrashJava() {
        return AwFeatureMap.isEnabled(AwFeatures.WEBVIEW_ENABLE_CRASH)
                && CommandLine.getInstance().hasSwitch(AwSwitches.WEBVIEW_FORCE_CRASH_JAVA);
    }

    public static boolean shouldCrashNative() {
        return AwFeatureMap.isEnabled(AwFeatures.WEBVIEW_ENABLE_CRASH)
                && CommandLine.getInstance().hasSwitch(AwSwitches.WEBVIEW_FORCE_CRASH_NATIVE);
    }

    // Do not instantiate this class.
    private AwCrashyClassUtils() {}

    @NativeMethods
    interface Natives {
        void crashInNative();
    }
}
