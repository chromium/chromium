// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.services;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.EITHER_PROCESS;

import android.content.Context;
import android.content.Intent;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.common.PlatformServiceBridge;
import org.chromium.android_webview.common.services.IMetricsUploadService;
import org.chromium.android_webview.services.MetricsUploadService;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.android_webview.test.MetricsTestPlatformServiceBridge;
import org.chromium.android_webview.test.OnlyRunIn;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.components.metrics.ChromeUserMetricsExtensionProtos.ChromeUserMetricsExtension;

import java.net.HttpURLConnection;

/**
 * Instrumentation tests MetricsUploadService. These tests are not batched to make sure all unbinded
 * services are properly killed between tests.
 */
@RunWith(AwJUnit4ClassRunner.class)
@OnlyRunIn(EITHER_PROCESS) // These tests don't use the renderer process
@Batch(Batch.PER_CLASS)
@MediumTest
public class MetricsUploadServiceTest {
    private ChromeUserMetricsExtension mMetricsLog;
    private MetricsTestPlatformServiceBridge mPlatformServiceBridge;

    @Before
    public void setUp() {
        mPlatformServiceBridge = new MetricsTestPlatformServiceBridge();
        PlatformServiceBridge.injectInstance(mPlatformServiceBridge);

        mMetricsLog = ChromeUserMetricsExtension.newBuilder().setClientId(123456789L).build();
    }

    @Test
    public void testLogSuccess_unchanged() throws Throwable {
        testLogStatus(0, HttpURLConnection.HTTP_OK);

        ChromeUserMetricsExtension receivedLog = mPlatformServiceBridge.waitForNextMetricsLog();
        Assert.assertEquals(mMetricsLog, receivedLog);
    }

    @Test
    public void testLogSuccess_cached() throws Throwable {
        testLogStatus(-1, HttpURLConnection.HTTP_OK);
    }

    @Test
    public void testLogFailure_internalError() throws Throwable {
        testLogStatus(8, HttpURLConnection.HTTP_INTERNAL_ERROR);
    }

    @Test
    public void testLogFailure_interrupted() throws Throwable {
        testLogStatus(14, HttpURLConnection.HTTP_UNAVAILABLE);
    }

    @Test
    public void testLogFailure_timeout() throws Throwable {
        testLogStatus(15, HttpURLConnection.HTTP_GATEWAY_TIMEOUT);
    }

    @MediumTest
    @Test
    public void testLogFailure_cancelled() throws Throwable {
        testLogStatus(16, HttpURLConnection.HTTP_GONE);
    }

    @Test
    public void testLogFailure_apiUnavailable() throws Throwable {
        testLogStatus(17, HttpURLConnection.HTTP_BAD_REQUEST);
    }

    @Test
    public void testLogFailure_unexpectedPlatformStatus() throws Throwable {
        testLogStatus(350, HttpURLConnection.HTTP_BAD_REQUEST);
    }

    private void testLogStatus(int platformStatusCode, int expectedHttpStatusCode)
            throws Throwable {
        mPlatformServiceBridge.setLogMetricsBlockingStatus(platformStatusCode);

        Intent intent =
                new Intent(ContextUtils.getApplicationContext(), MetricsUploadService.class);
        try (ServiceConnectionHelper helper =
                new ServiceConnectionHelper(intent, Context.BIND_AUTO_CREATE)) {
            IMetricsUploadService service =
                    IMetricsUploadService.Stub.asInterface(helper.getBinder());
            int returnedStatus = service.uploadMetricsLog(mMetricsLog.toByteArray());

            Assert.assertEquals(expectedHttpStatusCode, returnedStatus);
        }
    }
}
