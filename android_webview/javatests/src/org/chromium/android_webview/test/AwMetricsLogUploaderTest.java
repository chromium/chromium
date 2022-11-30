// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwBrowserProcess;
import org.chromium.android_webview.common.PlatformServiceBridge;
import org.chromium.android_webview.metrics.AwMetricsLogUploader;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.components.metrics.ChromeUserMetricsExtensionProtos.ChromeUserMetricsExtension;

import java.util.ArrayList;

/**
 * Instrumentation tests MetricsUploadService. These tests are not batched to make sure all unbinded
 * services are properly killed between tests.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class AwMetricsLogUploaderTest {
    private static final ChromeUserMetricsExtension SAMPLE_TEST_METRICS_LOG =
            ChromeUserMetricsExtension.newBuilder().setClientId(123456789L).build();

    private MetricsTestPlatformServiceBridge mPlatformServiceBridge;

    @Before
    public void setUp() {
        mPlatformServiceBridge = new MetricsTestPlatformServiceBridge();
        PlatformServiceBridge.injectInstance(mPlatformServiceBridge);
        ThreadUtils.runOnUiThread(() -> {
            AwBrowserProcess.setWebViewPackageName(
                    ContextUtils.getApplicationContext().getPackageName());
        });
    }

    @MediumTest
    @Test
    public void testSendingData_withPreBinding() throws Throwable {
        AwMetricsLogUploader uploader = new AwMetricsLogUploader();
        uploader.initialize();
        uploader.accept(SAMPLE_TEST_METRICS_LOG.toByteArray());

        ChromeUserMetricsExtension receivedLog = mPlatformServiceBridge.waitForNextMetricsLog();
        Assert.assertEquals(SAMPLE_TEST_METRICS_LOG, receivedLog);
    }

    @MediumTest
    @Test
    public void testSendingData_withoutPreBinding() throws Throwable {
        AwMetricsLogUploader uploader = new AwMetricsLogUploader();
        uploader.accept(SAMPLE_TEST_METRICS_LOG.toByteArray());
        ChromeUserMetricsExtension receivedLog = mPlatformServiceBridge.waitForNextMetricsLog();
        Assert.assertEquals(SAMPLE_TEST_METRICS_LOG, receivedLog);
    }

    @MediumTest
    @Test
    public void testSendingMultipleLogs() throws Throwable {
        AwMetricsLogUploader uploader = new AwMetricsLogUploader();
        uploader.initialize();

        final int numberOfLogs = 5;
        ChromeUserMetricsExtension[] expectedLogs = new ChromeUserMetricsExtension[numberOfLogs];
        for (int i = 0; i < numberOfLogs; i++) {
            expectedLogs[i] = ChromeUserMetricsExtension.newBuilder().setClientId(i + 1).build();
            uploader.accept(expectedLogs[i].toByteArray());
        }

        ArrayList<ChromeUserMetricsExtension> receivedLogs = new ArrayList<>();
        for (int i = 0; i < numberOfLogs; i++) {
            ChromeUserMetricsExtension log = mPlatformServiceBridge.waitForNextMetricsLog();
            receivedLogs.add(log);
        }

        // Metrics are sent in async background tasks that can be executed in any arbitrary order.
        Assert.assertThat(receivedLogs, Matchers.containsInAnyOrder(expectedLogs));
    }
}
