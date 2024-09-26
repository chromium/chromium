// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.metrics;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.android_webview.ManifestMetadataUtil;
import org.chromium.base.ThreadUtils;

/**
 * Determines user consent and app opt-out for metrics. See aw_metrics_service_client.h for more
 * explanation.
 */
@JNINamespace("android_webview")
public class AwMetricsServiceClient {
    /**
     * Set user consent settings.
     *
     * @param userConsent user consent via Android Usage & diagnostics settings.
     */
    public static void setConsentSetting(boolean userConsent) {
        ThreadUtils.assertOnUiThread();
        AwMetricsServiceClientJni.get()
                .setHaveMetricsConsent(
                        userConsent, !ManifestMetadataUtil.isAppOptedOutFromMetricsCollection());
    }

    public static void setFastStartupForTesting(boolean fastStartupForTesting) {
        AwMetricsServiceClientJni.get().setFastStartupForTesting(fastStartupForTesting);
    }

    public static void setUploadIntervalForTesting(long uploadIntervalMs) {
        AwMetricsServiceClientJni.get().setUploadIntervalForTesting(uploadIntervalMs);
    }

    /** Sets a callback to run each time after final metrics have been collected. */
    public static void setOnFinalMetricsCollectedListenerForTesting(Runnable listener) {
        AwMetricsServiceClientJni.get().setOnFinalMetricsCollectedListenerForTesting(listener);
    }

    @NativeMethods
    interface Natives {
        void setHaveMetricsConsent(boolean userConsent, boolean appConsent);

        void setFastStartupForTesting(boolean fastStartupForTesting);

        void setUploadIntervalForTesting(long uploadIntervalMs);

        void setOnFinalMetricsCollectedListenerForTesting(Runnable listener);
    }
}
