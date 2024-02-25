// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.services;

import android.app.Service;
import android.content.Intent;
import android.os.ConditionVariable;
import android.os.IBinder;

import org.junit.Assert;

import org.chromium.android_webview.common.services.IMetricsBridgeService;

import java.util.List;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Mock implementation for MetricsBridgeService that record metrics data and provide methods
 * for testing this data. Used in
 * {@link org.chromium.android_webview.test.common.metrics.AwNonembeddedUmaRecorderTest}.
 */
public class MockMetricsBridgeService extends Service {
    public static final long TIMEOUT_MILLIS = 10000;

    private static byte[] sRecordedData;
    private static final AtomicInteger sRecordsCount = new AtomicInteger(0);
    private static final ConditionVariable sReceivedRecord = new ConditionVariable();

    private final IMetricsBridgeService.Stub mMockBinder =
            new IMetricsBridgeService.Stub() {
                @Override
                public void recordMetrics(byte[] data) {
                    sRecordedData = data;
                    sRecordsCount.incrementAndGet();
                    sReceivedRecord.open();
                }

                @Override
                public List<byte[]> retrieveNonembeddedMetrics() {
                    throw new UnsupportedOperationException();
                }
            };

    @Override
    public IBinder onBind(Intent intent) {
        return mMockBinder;
    }

    /**
     * Block until the recordMetrics is called exactly for {@code times} number of times. Fail
     * after {@link TIMEOUT_MILLIS} milliseconds.
     *
     * @param times the number of times recordMetrics() is expected to be called
     * @return the byte data received by recordMetrics on the {@code times} call.
     */
    public static byte[] getReceivedDataAfter(int times) throws TimeoutException {
        while (sRecordsCount.get() < times) {
            sReceivedRecord.close();
            if (!sReceivedRecord.block(TIMEOUT_MILLIS)) {
                throw new TimeoutException(
                        "Timedout waiting for recordMetrics() to be called for the ("
                                + (sRecordsCount.get() + 1)
                                + ") time");
            }
        }
        Assert.assertEquals(
                "recordMetrics() should be called (" + times + ") times",
                times,
                sRecordsCount.get());
        return sRecordedData;
    }

    /** Reset static variables between tests. */
    public static void reset() {
        sRecordedData = null;
        sRecordsCount.set(0);
        sReceivedRecord.close();
    }
}
