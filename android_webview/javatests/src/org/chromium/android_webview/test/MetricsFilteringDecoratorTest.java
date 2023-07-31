// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.common.PlatformServiceBridge;
import org.chromium.android_webview.metrics.AwMetricsUtils;
import org.chromium.android_webview.metrics.MetricsFilteringDecorator;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.components.metrics.AndroidMetricsLogConsumer;
import org.chromium.components.metrics.ChromeUserMetricsExtensionProtos.ChromeUserMetricsExtension;
import org.chromium.components.metrics.HistogramEventProtos.HistogramEventProto;
import org.chromium.components.metrics.SystemProfileProtos.SystemProfileProto;
import org.chromium.components.metrics.SystemProfileProtos.SystemProfileProto.MetricsFilteringStatus;
import org.chromium.components.metrics.UserActionEventProtos.UserActionEventProto;

import java.net.HttpURLConnection;
import java.util.Arrays;

/**
 * Instrumentation tests for {@link MetricsFilteringDecorator}.
 */
@RunWith(AwJUnit4ClassRunner.class)
@MediumTest
@Batch(Batch.PER_CLASS)
public class MetricsFilteringDecoratorTest {
    @Rule
    public AwActivityTestRule mRule = new AwActivityTestRule();

    private MetricsTestPlatformServiceBridge mPlatformServiceBridge;
    private AndroidMetricsLogConsumer mUploader;

    @Before
    public void setUp() {
        mPlatformServiceBridge = new MetricsTestPlatformServiceBridge();
        PlatformServiceBridge.injectInstance(mPlatformServiceBridge);

        AndroidMetricsLogConsumer directUploader = data -> {
            PlatformServiceBridge.getInstance().logMetrics(data, true);
            return HttpURLConnection.HTTP_OK;
        };
        mUploader = new MetricsFilteringDecorator(directUploader);
    }

    private HistogramEventProto createHistogramWithName(String histogramName) {
        return HistogramEventProto.newBuilder()
                .setNameHash(AwMetricsUtils.hashHistogramName(histogramName))
                .build();
    }

    private UserActionEventProto getUserAction() {
        return UserActionEventProto.newBuilder()
                .setNameHash(6207116539890058931L)
                .setTimeSec(417309L)
                .build();
    }

    @Test
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add("enable-features=" + AwFeatures.WEBVIEW_METRICS_FILTERING)
    public void testMetricsFiltering_applied() throws Throwable {
        ChromeUserMetricsExtension log =
                ChromeUserMetricsExtension.newBuilder()
                        .setSystemProfile(SystemProfileProto.newBuilder().setMetricsFilteringStatus(
                                MetricsFilteringStatus.METRICS_ONLY_CRITICAL))
                        .addAllHistogramEvent(Arrays.asList(
                                createHistogramWithName("Android.WebView.Visibility.Global"),
                                createHistogramWithName("Histogram.Not.In.Allowlist"),
                                createHistogramWithName(
                                        "Android.WebView.SafeMode.SafeModeEnabled")))
                        .addUserActionEvent(getUserAction())
                        .build();

        ChromeUserMetricsExtension expectedReceivedLog =
                ChromeUserMetricsExtension.newBuilder()
                        .setSystemProfile(SystemProfileProto.newBuilder().setMetricsFilteringStatus(
                                MetricsFilteringStatus.METRICS_ONLY_CRITICAL))
                        .addAllHistogramEvent(Arrays.asList(
                                createHistogramWithName("Android.WebView.Visibility.Global"),
                                createHistogramWithName(
                                        "Android.WebView.SafeMode.SafeModeEnabled")))
                        .build();

        int status = mUploader.log(log.toByteArray());
        Assert.assertEquals(HttpURLConnection.HTTP_OK, status);
        ChromeUserMetricsExtension receivedLog = mPlatformServiceBridge.waitForNextMetricsLog();
        Assert.assertEquals(expectedReceivedLog, receivedLog);
    }

    @Test
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add("enable-features=" + AwFeatures.WEBVIEW_METRICS_FILTERING)
    public void testMetricsFiltering_notApplied() throws Throwable {
        ChromeUserMetricsExtension log =
                ChromeUserMetricsExtension.newBuilder()
                        .setSystemProfile(SystemProfileProto.newBuilder().setMetricsFilteringStatus(
                                MetricsFilteringStatus.METRICS_ALL))
                        .addHistogramEvent(createHistogramWithName("Histogram.Not.In.Allowlist"))
                        .addUserActionEvent(getUserAction())
                        .build();

        int status = mUploader.log(log.toByteArray());
        Assert.assertEquals(HttpURLConnection.HTTP_OK, status);
        ChromeUserMetricsExtension receivedLog = mPlatformServiceBridge.waitForNextMetricsLog();
        // We expect to receive an identical log to the one we provided.
        Assert.assertEquals(log, receivedLog);
    }

    @Test
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add("enable-features=" + AwFeatures.WEBVIEW_METRICS_FILTERING)
    public void testMetricsFiltering_missingSystemProfile() throws Throwable {
        ChromeUserMetricsExtension log =
                ChromeUserMetricsExtension.newBuilder()
                        .addHistogramEvent(createHistogramWithName("Histogram.Not.In.Allowlist"))
                        .addUserActionEvent(getUserAction())
                        .build();

        int status = mUploader.log(log.toByteArray());
        Assert.assertEquals(HttpURLConnection.HTTP_OK, status);
        ChromeUserMetricsExtension receivedLog = mPlatformServiceBridge.waitForNextMetricsLog();
        // We expect to receive an identical log to the one we provided.
        Assert.assertEquals(log, receivedLog);
    }

    @Test
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add("disable-features=" + AwFeatures.WEBVIEW_METRICS_FILTERING)
    public void testMetricsFiltering_featureOff() throws Throwable {
        // Note: It should not typically be the case that a METRICS_ONLY_CRITICAL log is
        // processed while the feature is off, however this can happen in the following
        // scenario:
        // 1. Feature is enabled for some clients.
        // 2. Clients generate METRICS_ONLY_CRITICAL logs, but not all of them are sent (e.g. no
        // network connection)
        // 3. Feature gets disabled for those clients (e.g. server-side config change)
        // 4. Clients go to upload previously generated logs
        //
        // This is an edge case that would happen to a small subset of logs in the case
        // the feature gets disabled. Although the above results in METRICS_ONLY_CRITICAL
        // logs containing full data, we are OK with that behavior given the feature check
        // on the Java side ensures we are able to evaluate the performance impact of
        // the proto parsing code.
        ChromeUserMetricsExtension log =
                ChromeUserMetricsExtension.newBuilder()
                        .setSystemProfile(SystemProfileProto.newBuilder().setMetricsFilteringStatus(
                                MetricsFilteringStatus.METRICS_ONLY_CRITICAL))
                        .addHistogramEvent(createHistogramWithName("Histogram.Not.In.Allowlist"))
                        .addUserActionEvent(getUserAction())
                        .build();

        int status = mUploader.log(log.toByteArray());
        Assert.assertEquals(HttpURLConnection.HTTP_OK, status);
        ChromeUserMetricsExtension receivedLog = mPlatformServiceBridge.waitForNextMetricsLog();
        // We expect to receive an identical log to the one we provided.
        Assert.assertEquals(log, receivedLog);
    }
}
