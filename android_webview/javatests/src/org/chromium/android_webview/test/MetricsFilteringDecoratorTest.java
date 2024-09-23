// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.EITHER_PROCESS;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.common.PlatformServiceBridge;
import org.chromium.android_webview.metrics.AwMetricsUtils;
import org.chromium.android_webview.metrics.MetricsFilteringDecorator;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.components.metrics.AndroidMetricsLogConsumer;
import org.chromium.components.metrics.ChromeUserMetricsExtensionProtos.ChromeUserMetricsExtension;
import org.chromium.components.metrics.HistogramEventProtos.HistogramEventProto;
import org.chromium.components.metrics.SystemProfileProtos.SystemProfileProto;
import org.chromium.components.metrics.SystemProfileProtos.SystemProfileProto.MetricsFilteringStatus;
import org.chromium.components.metrics.UserActionEventProtos.UserActionEventProto;

import java.net.HttpURLConnection;
import java.util.Arrays;

/** Instrumentation tests for {@link MetricsFilteringDecorator}. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@OnlyRunIn(EITHER_PROCESS) // These tests don't use the renderer process
@MediumTest
@Batch(Batch.PER_CLASS)
public class MetricsFilteringDecoratorTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mRule;

    private MetricsTestPlatformServiceBridge mPlatformServiceBridge;
    private AndroidMetricsLogConsumer mUploader;

    public MetricsFilteringDecoratorTest(AwSettingsMutation param) {
        this.mRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() {
        mPlatformServiceBridge = new MetricsTestPlatformServiceBridge();
        PlatformServiceBridge.injectInstance(mPlatformServiceBridge);

        AndroidMetricsLogConsumer directUploader =
                data -> {
                    PlatformServiceBridge.getInstance().logMetrics(data);
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
    public void testMetricsFiltering_applied() throws Throwable {
        ChromeUserMetricsExtension log =
                ChromeUserMetricsExtension.newBuilder()
                        .setSystemProfile(
                                SystemProfileProto.newBuilder()
                                        .setMetricsFilteringStatus(
                                                MetricsFilteringStatus.METRICS_ONLY_CRITICAL))
                        .addAllHistogramEvent(
                                Arrays.asList(
                                        createHistogramWithName(
                                                "Android.WebView.Visibility.Global"),
                                        createHistogramWithName("Histogram.Not.In.Allowlist"),
                                        createHistogramWithName(
                                                "Android.WebView.SafeMode.SafeModeEnabled")))
                        .addUserActionEvent(getUserAction())
                        .build();

        ChromeUserMetricsExtension expectedReceivedLog =
                ChromeUserMetricsExtension.newBuilder()
                        .setSystemProfile(
                                SystemProfileProto.newBuilder()
                                        .setMetricsFilteringStatus(
                                                MetricsFilteringStatus.METRICS_ONLY_CRITICAL))
                        .addAllHistogramEvent(
                                Arrays.asList(
                                        createHistogramWithName(
                                                "Android.WebView.Visibility.Global"),
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
    public void testMetricsFiltering_notApplied() throws Throwable {
        ChromeUserMetricsExtension log =
                ChromeUserMetricsExtension.newBuilder()
                        .setSystemProfile(
                                SystemProfileProto.newBuilder()
                                        .setMetricsFilteringStatus(
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
}
