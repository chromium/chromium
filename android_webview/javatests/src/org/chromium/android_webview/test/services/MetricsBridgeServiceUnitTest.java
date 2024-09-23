// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.services;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.EITHER_PROCESS;

import android.os.IBinder;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.common.services.IMetricsBridgeService;
import org.chromium.android_webview.proto.MetricsBridgeRecords.HistogramRecord;
import org.chromium.android_webview.proto.MetricsBridgeRecords.HistogramRecord.RecordType;
import org.chromium.android_webview.services.MetricsBridgeService;
import org.chromium.android_webview.test.AwActivityTestRule;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.android_webview.test.OnlyRunIn;
import org.chromium.base.FileUtils;
import org.chromium.base.test.util.Batch;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.util.List;
import java.util.concurrent.FutureTask;

/**
 * Instrumentation tests for MetricsBridgeService. These tests are batched as UNIT_TESTS because
 * they don't actually launch any services or other components.
 */
@RunWith(AwJUnit4ClassRunner.class)
@OnlyRunIn(EITHER_PROCESS) // These are unit tests
@Batch(Batch.UNIT_TESTS)
public class MetricsBridgeServiceUnitTest {
    public static final byte[] PARSING_LOG_RESULT_SUCCESS_RECORD =
            HistogramRecord.newBuilder()
                    .setRecordType(RecordType.HISTOGRAM_LINEAR)
                    .setHistogramName("Android.WebView.NonEmbeddedMetrics.ParsingLogResult")
                    .setSample(MetricsBridgeService.ParsingLogResult.SUCCESS)
                    .setMin(1)
                    .setMax(MetricsBridgeService.ParsingLogResult.COUNT)
                    .setNumBuckets(MetricsBridgeService.ParsingLogResult.COUNT + 1)
                    .build()
                    .toByteArray();

    private File mTempFile;

    @Before
    public void setUp() throws IOException {
        mTempFile = File.createTempFile("test_webview_metrics_bridge_logs", null);
    }

    @After
    public void tearDown() {
        if (mTempFile.exists()) {
            Assert.assertTrue("Failed to delete \"" + mTempFile + "\"", mTempFile.delete());
        }
    }

    @Test
    @MediumTest
    // Test that the service saves metrics records to file
    public void testSaveToFile() throws Throwable {
        HistogramRecord recordBooleanProto =
                HistogramRecord.newBuilder()
                        .setRecordType(RecordType.HISTOGRAM_BOOLEAN)
                        .setHistogramName("testSaveToFile.boolean")
                        .setSample(1)
                        .build();
        HistogramRecord recordLinearProto =
                HistogramRecord.newBuilder()
                        .setRecordType(RecordType.HISTOGRAM_LINEAR)
                        .setHistogramName("testSaveToFile.linear")
                        .setSample(123)
                        .setMin(1)
                        .setMax(1000)
                        .setNumBuckets(50)
                        .build();
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        writeRecordsToStream(out, recordBooleanProto, recordLinearProto, recordBooleanProto);
        byte[] expectedData = out.toByteArray();

        // Cannot bind to service using real connection since we need to inject test file name.
        MetricsBridgeService service = new MetricsBridgeService(mTempFile);
        // Simulate starting the service by calling onCreate()
        service.onCreate();

        IBinder binder = service.onBind(null);
        IMetricsBridgeService stub = IMetricsBridgeService.Stub.asInterface(binder);
        stub.recordMetrics(recordBooleanProto.toByteArray());
        stub.recordMetrics(recordLinearProto.toByteArray());
        stub.recordMetrics(recordBooleanProto.toByteArray());

        // Block until all tasks are finished to make sure all records are written to file.
        FutureTask<Object> blockTask = service.addTaskToBlock();
        AwActivityTestRule.waitForFuture(blockTask);

        byte[] resultData = FileUtils.readStream(new FileInputStream(mTempFile));
        Assert.assertArrayEquals(
                "byte data from file is different from the expected proto byte data",
                expectedData,
                resultData);
    }

    @Test
    @MediumTest
    // Test that service recovers saved data from file, appends new records to it and
    // clears the file after a retrieve call.
    public void testRetrieveFromFile() throws Throwable {
        HistogramRecord recordBooleanProto =
                HistogramRecord.newBuilder()
                        .setRecordType(RecordType.HISTOGRAM_BOOLEAN)
                        .setHistogramName("testRecoverFromFile.boolean")
                        .setSample(1)
                        .build();
        HistogramRecord recordLinearProto =
                HistogramRecord.newBuilder()
                        .setRecordType(RecordType.HISTOGRAM_LINEAR)
                        .setHistogramName("testRecoverFromFile.linear")
                        .setSample(123)
                        .setMin(1)
                        .setMax(1000)
                        .setNumBuckets(50)
                        .build();
        // write Initial proto data To File
        writeRecordsToStream(
                new FileOutputStream(mTempFile),
                recordBooleanProto,
                recordLinearProto,
                recordBooleanProto);

        // Cannot bind to service using real connection since we need to inject test file name.
        MetricsBridgeService service = new MetricsBridgeService(mTempFile);
        // Simulate starting the service by calling onCreate()
        service.onCreate();

        IBinder binder = service.onBind(null);
        IMetricsBridgeService stub = IMetricsBridgeService.Stub.asInterface(binder);
        stub.recordMetrics(recordBooleanProto.toByteArray());
        List<byte[]> retrievedDataList = stub.retrieveNonembeddedMetrics();

        byte[][] expectedData =
                new byte[][] {
                    recordBooleanProto.toByteArray(),
                    recordLinearProto.toByteArray(),
                    recordBooleanProto.toByteArray(),
                    PARSING_LOG_RESULT_SUCCESS_RECORD,
                    recordBooleanProto.toByteArray()
                };

        // Assert file is deleted after the retrieve call
        Assert.assertFalse(
                "file should be deleted after retrieve metrics call", mTempFile.exists());
        Assert.assertNotNull("retrieved byte data from the service is null", retrievedDataList);
        Assert.assertArrayEquals(
                "retrieved byte data is different from the expected data",
                expectedData,
                retrievedDataList.toArray());
    }

    private static void writeRecordsToStream(OutputStream os, HistogramRecord... records)
            throws IOException {
        for (HistogramRecord record : records) {
            record.writeDelimitedTo(os);
        }
        os.close();
    }
}
