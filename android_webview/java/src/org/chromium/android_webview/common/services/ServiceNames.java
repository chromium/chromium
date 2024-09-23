// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common.services;

/**
 * Defines constants containing the fully-qualified names of WebView services.
 *
 * <p>This class exists to avoid having to depend on service classes just to get their name. Note
 * that it is safe to launch a Service just by its name: Service names can never be obfuscated so we
 * can rely on the full name to stay the same.
 */
public class ServiceNames {
    public static final String AW_MINIDUMP_UPLOAD_JOB_SERVICE =
            "org.chromium.android_webview.services.AwMinidumpUploadJobService";
    public static final String CRASH_RECEIVER_SERVICE =
            "org.chromium.android_webview.services.CrashReceiverService";
    public static final String DEVELOPER_MODE_CONTENT_PROVIDER =
            "org.chromium.android_webview.services.DeveloperModeContentProvider";
    public static final String DEVELOPER_UI_SERVICE =
            "org.chromium.android_webview.services.DeveloperUiService";
    public static final String METRICS_BRIDGE_SERVICE =
            "org.chromium.android_webview.services.MetricsBridgeService";
    public static final String METRICS_UPLOAD_SERVICE =
            "org.chromium.android_webview.services.MetricsUploadService";
    public static final String NET_LOG_SERVICE =
            "org.chromium.android_webview.services.AwNetLogService";
    public static final String VARIATIONS_SEED_SERVER =
            "org.chromium.android_webview.services.VariationsSeedServer";
    public static final String AW_COMPONENT_UPDATE_SERVICE =
            "org.chromium.android_webview.nonembedded.AwComponentUpdateService";

    private ServiceNames() {}
}
