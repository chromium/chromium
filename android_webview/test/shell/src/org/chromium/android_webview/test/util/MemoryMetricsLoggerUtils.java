// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.util;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/** Utilities for force recording memory metrics. */
@JNINamespace("android_webview")
public class MemoryMetricsLoggerUtils {
    @NativeMethods
    public interface Natives {
        /**
         * Calls to MemoryMetricsLogger to force recording histograms, returning true on success.
         * A value of false means recording failed (most likely because process metrics not
         * available.
         */
        boolean forceRecordHistograms();
    }
}
