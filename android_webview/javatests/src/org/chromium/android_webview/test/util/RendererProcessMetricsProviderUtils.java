// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.util;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/** Utilities for force recording renderer process metrics. */
@JNINamespace("android_webview")
public class RendererProcessMetricsProviderUtils {
    @NativeMethods
    public interface Natives {
        /** Calls to RendererProcessMetricsProvider to force record histograms. */
        void forceRecordHistograms();
    }
}
