// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.services;

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
import org.chromium.base.ContextUtils;
import org.chromium.components.metrics.ChromeUserMetricsExtensionProtos.ChromeUserMetricsExtension;

/**
 * Instrumentation tests MetricsUploadService. These tests are not batched to make sure all unbinded
 * services are properly killed between tests.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class MetricsUploadServiceTest {
    private static final long TEST_CLIENT_ID = 123456789L;

    private TestPlatformServiceBridge mPlatformServiceBridge;

    private static class TestPlatformServiceBridge extends PlatformServiceBridge {
        private byte[] mMetricsLogData;

        @Override
        public void logMetrics(byte[] data) {
            mMetricsLogData = data;
        }

        public byte[] getLoggedMetricsData() {
            return mMetricsLogData;
        }
    }

    @Before
    public void setUp() {
        mPlatformServiceBridge = new TestPlatformServiceBridge();
        PlatformServiceBridge.injectInstance(mPlatformServiceBridge);
    }

    @MediumTest
    @Test
    public void testLogUnchagned() throws Throwable {
        byte[] expectedData = ChromeUserMetricsExtension.newBuilder()
                                      .setClientId(TEST_CLIENT_ID)
                                      .build()
                                      .toByteArray();

        Intent intent =
                new Intent(ContextUtils.getApplicationContext(), MetricsUploadService.class);
        try (ServiceConnectionHelper helper =
                        new ServiceConnectionHelper(intent, Context.BIND_AUTO_CREATE)) {
            IMetricsUploadService service =
                    IMetricsUploadService.Stub.asInterface(helper.getBinder());
            service.uploadMetricsLog(expectedData);

            byte[] receivedData = mPlatformServiceBridge.getLoggedMetricsData();
            Assert.assertNotNull("received byte data is null", receivedData);
            Assert.assertArrayEquals("received byte data is different from the expected data",
                    expectedData, receivedData);
        }
    }
}
