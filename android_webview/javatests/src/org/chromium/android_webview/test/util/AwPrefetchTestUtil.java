// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.util;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

/** Test utility for `AwPrefetchTest`. */
@JNINamespace("android_webview")
public class AwPrefetchTestUtil {
    public static void setLatestPrefetchInfoForTesting(String origin, boolean javascriptEnabled) {
        AwPrefetchTestUtilJni.get().setLatestPrefetchInfoForTesting(origin, javascriptEnabled);
    }

    public static void clearLatestPrefetchInfoForTesting() {
        AwPrefetchTestUtilJni.get().clearLatestPrefetchInfoForTesting();
    }

    @NativeMethods
    interface Natives {
        void setLatestPrefetchInfoForTesting(
                @JniType("std::string") String origin, boolean javascriptEnabled);

        void clearLatestPrefetchInfoForTesting();
    }
}
