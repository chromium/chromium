// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

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
import org.chromium.base.test.util.Batch;
import org.chromium.components.metrics.ChromeUserMetricsExtensionProtos.ChromeUserMetricsExtension;

import java.net.HttpURLConnection;
import java.util.ArrayList;
import java.util.concurrent.CancellationException;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * Instrumentation tests MetricsUploadService. These tests are not batched to make sure all unbinded
 * services are properly killed between tests.
 */
@RunWith(AwJUnit4ClassRunner.class)
@MediumTest
@Batch(Batch.PER_CLASS)
public class AwMetricsLogUploaderTest {
    private static final ChromeUserMetricsExtension SAMPLE_TEST_METRICS_LOG =
            ChromeUserMetricsExtension.newBuilder().setClientId(123456789L).build();
    private static final ChromeUserMetricsExtension EMPTY_TEST_METRICS_LOG =
            ChromeUserMetricsExtension.newBuilder().build();

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

    @Test
    public void testSendingData_withPreBinding() throws Throwable {
        AwMetricsLogUploader uploader = new AwMetricsLogUploader(
                /* waitForResults= */ true, /* useDefaultUploadQos= */ false);
        uploader.initialize();
        int status = uploader.log(SAMPLE_TEST_METRICS_LOG.toByteArray());
        Assert.assertEquals(HttpURLConnection.HTTP_OK, status);

        ChromeUserMetricsExtension receivedLog = mPlatformServiceBridge.waitForNextMetricsLog();
        Assert.assertEquals(SAMPLE_TEST_METRICS_LOG, receivedLog);
    }

    @Test
    public void testSendingData_withoutPreBinding() throws Throwable {
        AwMetricsLogUploader uploader = new AwMetricsLogUploader(
                /* waitForResults= */ true, /* useDefaultUploadQos= */ false);
        int status = uploader.log(SAMPLE_TEST_METRICS_LOG.toByteArray());
        Assert.assertEquals(HttpURLConnection.HTTP_OK, status);
        ChromeUserMetricsExtension receivedLog = mPlatformServiceBridge.waitForNextMetricsLog();
        Assert.assertEquals(SAMPLE_TEST_METRICS_LOG, receivedLog);
    }

    @Test
    public void testSendingData_setsDefaultUploadQos() throws Throwable {
        testDefaultUploadQosFlag(true);
        testDefaultUploadQosFlag(false);
    }

    private void testDefaultUploadQosFlag(boolean expectedValue) throws Throwable {
        AwMetricsLogUploader uploader = new AwMetricsLogUploader(
                /* waitForResults= */ true, /* useDefaultUploadQos= */ expectedValue);
        uploader.log(SAMPLE_TEST_METRICS_LOG.toByteArray());
        mPlatformServiceBridge.waitForNextMetricsLog();

        Assert.assertEquals(expectedValue, mPlatformServiceBridge.isLastLogUsingDefaultUploadQos());
    }

    @Test
    public void testSendingDataException_whenCancellationException() throws Throwable {
        testSendingDataException(new CancellationException(), HttpURLConnection.HTTP_GONE);
    }

    @Test
    public void testSendingDataException_whenInterruptedException() throws Throwable {
        testSendingDataException(new InterruptedException(), HttpURLConnection.HTTP_UNAVAILABLE);
    }

    @Test
    public void testSendingDataException_whenExecutionException() throws Throwable {
        testSendingDataException(
                new ExecutionException("", null), HttpURLConnection.HTTP_BAD_REQUEST);
    }

    @Test
    public void testSendingDataException_whenTimesOut() throws Throwable {
        testSendingDataException(new TimeoutException(), HttpURLConnection.HTTP_CLIENT_TIMEOUT);
    }

    private void testSendingDataException(Throwable exceptionThrown, int expectedStatus)
            throws Throwable {
        // When the AwMetricsLogUploader attempts to wait for log results, an exception of our
        // choosing will be thrown.
        final CompletableFuture<Integer> mockedResultsFuture = mock(CompletableFuture.class);
        when(mockedResultsFuture.get(anyLong(), any())).thenAnswer(invocation -> {
            throw exceptionThrown;
        });

        AwMetricsLogUploader uploader = new AwMetricsLogUploader(
                /* waitForResults= */ true, /* useDefaultUploadQos= */ false);
        int status = uploader.log(SAMPLE_TEST_METRICS_LOG.toByteArray(), mockedResultsFuture);

        Assert.assertEquals(expectedStatus, status);
    }

    @Test
    public void testSendingData_platformStatusCodeIgnored_ifNotWaitingForResults()
            throws Throwable {
        // Returning a non 0 status code to simulate a failure.
        // We should still get a success code since this code is not waiting for this.
        mPlatformServiceBridge.setLogMetricsBlockingStatus(1);

        AwMetricsLogUploader uploader = new AwMetricsLogUploader(
                /* waitForResults= */ false, /* useDefaultUploadQos= */ false);
        int status = uploader.log(SAMPLE_TEST_METRICS_LOG.toByteArray());
        Assert.assertEquals(HttpURLConnection.HTTP_OK, status);
    }

    @Test
    public void testSendingMultipleLogs() throws Throwable {
        AwMetricsLogUploader uploader = new AwMetricsLogUploader(
                /* waitForResults= */ true, /* useDefaultUploadQos= */ false);
        uploader.initialize();

        final int numberOfLogs = 5;
        ChromeUserMetricsExtension[] expectedLogs = new ChromeUserMetricsExtension[numberOfLogs];
        for (int i = 0; i < numberOfLogs; i++) {
            expectedLogs[i] = ChromeUserMetricsExtension.newBuilder().setClientId(i + 1).build();
            int status = uploader.log(expectedLogs[i].toByteArray());
            Assert.assertEquals(HttpURLConnection.HTTP_OK, status);
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
