// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.services;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.EITHER_PROCESS;

import android.content.Context;
import android.content.Intent;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.common.services.IMetricsBridgeService;
import org.chromium.android_webview.proto.MetricsBridgeRecords.HistogramRecord;
import org.chromium.android_webview.proto.MetricsBridgeRecords.HistogramRecord.RecordType;
import org.chromium.android_webview.services.MetricsBridgeService;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.android_webview.test.OnlyRunIn;
import org.chromium.base.ContextUtils;

import java.util.List;

/**
 * Instrumentation tests MetricsBridgeService. These tests are not batched to make sure all unbinded
 * services are properly killed between tests.
 */
@RunWith(AwJUnit4ClassRunner.class)
@OnlyRunIn(EITHER_PROCESS) // These tests don't use the renderer process
public class MetricsBridgeServiceTest {
    @Test
    @MediumTest
    // Test sending data to the service and retrieving it back.
    public void testRecordAndRetrieveNonembeddedMetrics() throws Throwable {
        HistogramRecord recordProto =
                HistogramRecord.newBuilder()
                        .setRecordType(RecordType.HISTOGRAM_BOOLEAN)
                        .setHistogramName("testRecordAndRetrieveNonembeddedMetrics")
                        .setSample(1)
                        .build();
        byte[][] expectedData = new byte[][] {recordProto.toByteArray()};

        Intent intent =
                new Intent(ContextUtils.getApplicationContext(), MetricsBridgeService.class);
        try (ServiceConnectionHelper helper =
                new ServiceConnectionHelper(intent, Context.BIND_AUTO_CREATE)) {
            IMetricsBridgeService service =
                    IMetricsBridgeService.Stub.asInterface(helper.getBinder());
            service.recordMetrics(recordProto.toByteArray());
            List<byte[]> retrievedDataList = service.retrieveNonembeddedMetrics();

            Assert.assertArrayEquals(
                    "retrieved byte data is different from the expected data",
                    expectedData,
                    retrievedDataList.toArray());
        }
    }

    @Test
    @MediumTest
    // Test sending data to the service and retrieving it back and make sure it's cleared.
    public void testClearAfterRetrieveNonembeddedMetrics() throws Throwable {
        HistogramRecord recordProto =
                HistogramRecord.newBuilder()
                        .setRecordType(RecordType.HISTOGRAM_BOOLEAN)
                        .setHistogramName("testClearAfterRetrieveNonembeddedMetrics")
                        .setSample(1)
                        .build();
        byte[][] expectedData = new byte[][] {recordProto.toByteArray()};

        Intent intent =
                new Intent(ContextUtils.getApplicationContext(), MetricsBridgeService.class);
        try (ServiceConnectionHelper helper =
                new ServiceConnectionHelper(intent, Context.BIND_AUTO_CREATE)) {
            IMetricsBridgeService service =
                    IMetricsBridgeService.Stub.asInterface(helper.getBinder());
            service.recordMetrics(recordProto.toByteArray());
            List<byte[]> retrievedDataList = service.retrieveNonembeddedMetrics();

            Assert.assertNotNull("retrieved byte data from the service is null", retrievedDataList);
            Assert.assertArrayEquals(
                    "retrieved byte data is different from the expected data",
                    expectedData,
                    retrievedDataList.toArray());

            // Retrieve data a second time to make sure it has been cleared after the first call
            retrievedDataList = service.retrieveNonembeddedMetrics();

            Assert.assertArrayEquals(
                    "metrics kept by the service hasn't been cleared",
                    new byte[][] {},
                    retrievedDataList.toArray());
        }
    }
}
