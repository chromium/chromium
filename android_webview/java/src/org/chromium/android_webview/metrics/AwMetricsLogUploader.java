// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.metrics;

import org.chromium.android_webview.common.PlatformServiceBridge;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Passes UMA logs from native to PlatformServiceBridge.
 */
@JNINamespace("android_webview")
public class AwMetricsLogUploader {
    @CalledByNative
    public static void uploadLog(byte[] data) {
        // This relies on WebViewChromiumFactoryProvider having already created the
        // PlatformServiceBridge. This is guaranteed because metrics won't start until the
        // PlatformServiceBridge.queryMetricsSetting() callback fires.
        PlatformServiceBridge.getInstance().logMetrics(data);
    }
}
