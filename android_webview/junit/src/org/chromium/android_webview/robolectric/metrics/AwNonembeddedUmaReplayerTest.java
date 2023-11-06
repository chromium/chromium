// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.robolectric.metrics;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.android_webview.metrics.AwNonembeddedUmaReplayer;
import org.chromium.android_webview.proto.MetricsBridgeRecords.HistogramRecord;
import org.chromium.android_webview.proto.MetricsBridgeRecords.HistogramRecord.RecordType;
import org.chromium.base.metrics.UmaRecorder;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Test AwNonembeddedUmaReplayer. */
@RunWith(BaseRobolectricTestRunner.class)
@SuppressWarnings("DoNotMock") // Ok to mock UmaRecorder since this is testing metrics.
public class AwNonembeddedUmaReplayerTest {
    @Mock private UmaRecorder mUmaRecorder;

    @Before
    public void initMocks() {
        MockitoAnnotations.initMocks(this);
        UmaRecorderHolder.setNonNativeDelegate(mUmaRecorder);
    }

    @Test
    @SmallTest
    public void testReplayBooleanHistogram() {
        String histogramName = "testReplayTrueBooleanHistogram";
        HistogramRecord trueHistogramProto =
                HistogramRecord.newBuilder()
                        .setRecordType(RecordType.HISTOGRAM_BOOLEAN)
                        .setHistogramName(histogramName)
                        .setSample(1)
                        .build();
        HistogramRecord falseHistogramProto = trueHistogramProto.toBuilder().setSample(0).build();
        HistogramRecord inValidHistogramProto =
                trueHistogramProto.toBuilder().setSample(55).build();
        AwNonembeddedUmaReplayer.replayMethodCall(trueHistogramProto);
        AwNonembeddedUmaReplayer.replayMethodCall(trueHistogramProto);
        AwNonembeddedUmaReplayer.replayMethodCall(falseHistogramProto);
        AwNonembeddedUmaReplayer.replayMethodCall(inValidHistogramProto);
        verify(mUmaRecorder, times(2)).recordBooleanHistogram(histogramName, true);
        verify(mUmaRecorder, times(1)).recordBooleanHistogram(histogramName, false);
    }

    @Test
    @SmallTest
    public void testReplayExponentialHistogram() {
        String histogramName = "testReplayExponentialHistogram";
        int sample = 100;
        int min = 5;
        int max = 1000;
        int numBuckets = 20;
        HistogramRecord recordProto =
                HistogramRecord.newBuilder()
                        .setRecordType(RecordType.HISTOGRAM_EXPONENTIAL)
                        .setHistogramName(histogramName)
                        .setSample(sample)
                        .setMin(min)
                        .setMax(max)
                        .setNumBuckets(numBuckets)
                        .build();
        AwNonembeddedUmaReplayer.replayMethodCall(recordProto);
        verify(mUmaRecorder)
                .recordExponentialHistogram(histogramName, sample, min, max, numBuckets);
    }

    @Test
    @SmallTest
    public void testReplayLinearHistogram() {
        String histogramName = "testReplayLinearHistogram";
        int sample = 100;
        int min = 5;
        int max = 1000;
        int numBuckets = 20;
        HistogramRecord recordProto =
                HistogramRecord.newBuilder()
                        .setRecordType(RecordType.HISTOGRAM_LINEAR)
                        .setHistogramName(histogramName)
                        .setSample(sample)
                        .setMin(min)
                        .setMax(max)
                        .setNumBuckets(numBuckets)
                        .build();
        AwNonembeddedUmaReplayer.replayMethodCall(recordProto);
        verify(mUmaRecorder).recordLinearHistogram(histogramName, sample, min, max, numBuckets);
    }

    @Test
    @SmallTest
    public void testReplaySparseHistogram() {
        String histogramName = "testReplaySparseHistogram";
        int sample = 10;
        HistogramRecord recordProto =
                HistogramRecord.newBuilder()
                        .setRecordType(RecordType.HISTOGRAM_SPARSE)
                        .setHistogramName(histogramName)
                        .setSample(sample)
                        .build();
        AwNonembeddedUmaReplayer.replayMethodCall(recordProto);
        verify(mUmaRecorder).recordSparseHistogram(histogramName, sample);
    }

    @Test
    @SmallTest
    public void testReplayUserAction() {
        String histogramName = "testReplayUserAction";
        long elapsedRealtimeMillis = 123456789101112L;
        HistogramRecord recordProto =
                HistogramRecord.newBuilder()
                        .setRecordType(RecordType.USER_ACTION)
                        .setHistogramName(histogramName)
                        .setElapsedRealtimeMillis(elapsedRealtimeMillis)
                        .build();
        AwNonembeddedUmaReplayer.replayMethodCall(recordProto);
        verify(mUmaRecorder).recordUserAction(histogramName, elapsedRealtimeMillis);
    }
}
