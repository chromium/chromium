// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.android_webview.metrics;

import android.os.Bundle;

import org.chromium.android_webview.proto.MetricsBridgeRecords.HistogramRecord;
import org.chromium.android_webview.proto.MetricsBridgeRecords.HistogramRecord.RecordType;
import org.chromium.base.Log;
import org.chromium.base.metrics.UmaRecorderHolder;

/**
 * Replay the recorded method calls recorded by {@link AwProcessUmaRecorder}.
 *
 * Should be used in processes which have initialized Uma, such as the browser process.
 */
public class AwNonembeddedUmaReplayer {
    private static final String TAG = "AwNonembedUmaReplay";

    /**
     * Extract method arguments from the given {@link HistogramRecord} and call
     * {@link org.chromium.base.metrics.UmaRecorder#recordBooleanHistogram}.
     */
    private static void replayBooleanHistogram(HistogramRecord proto) {
        assert proto.getRecordType() == RecordType.HISTOGRAM_BOOLEAN;

        int sample = proto.getSample();
        if (sample != 0 && sample != 1) {
            Log.d(TAG, "Expected BooleanHistogram to have sample of 0 or 1, but was " + sample);
            return;
        }

        UmaRecorderHolder.get()
                .recordBooleanHistogram(proto.getHistogramName(), proto.getSample() != 0);
    }

    /**
     * Extract method arguments from the given {@link HistogramRecord} and call
     * {@link org.chromium.base.metrics.UmaRecorder#recordExponentialHistogram}.
     */
    private static void replayExponentialHistogram(HistogramRecord proto) {
        assert proto.getRecordType() == RecordType.HISTOGRAM_EXPONENTIAL;

        UmaRecorderHolder.get()
                .recordExponentialHistogram(
                        proto.getHistogramName(),
                        proto.getSample(),
                        proto.getMin(),
                        proto.getMax(),
                        proto.getNumBuckets());
    }

    /**
     * Extract method arguments from the given {@link HistogramRecord} and call
     * {@link org.chromium.base.metrics.UmaRecorder#recordLinearHistogram}.
     */
    private static void replayLinearHistogram(HistogramRecord proto) {
        assert proto.getRecordType() == RecordType.HISTOGRAM_LINEAR;

        UmaRecorderHolder.get()
                .recordLinearHistogram(
                        proto.getHistogramName(),
                        proto.getSample(),
                        proto.getMin(),
                        proto.getMax(),
                        proto.getNumBuckets());
    }

    /**
     * Extract method arguments from the given {@link HistogramRecord} and call
     * {@link org.chromium.base.metrics.UmaRecorder#recordSparseHistogram}.
     */
    private static void replaySparseHistogram(HistogramRecord proto) {
        assert proto.getRecordType() == RecordType.HISTOGRAM_SPARSE;

        UmaRecorderHolder.get().recordSparseHistogram(proto.getHistogramName(), proto.getSample());
    }

    /**
     * Extract method arguments from the given {@link HistogramRecord} and call
     * {@link org.chromium.base.metrics.UmaRecorder#recordUserAction}.
     */
    private static void replayUserAction(HistogramRecord proto) {
        assert proto.getRecordType() == RecordType.USER_ACTION;

        UmaRecorderHolder.get()
                .recordUserAction(proto.getHistogramName(), proto.getElapsedRealtimeMillis());
    }

    /**
     * Replay UMA API call record by calling that API method.
     *
     * @param methodCall {@link Bundle} that contains the UMA API type and arguments.
     */
    public static void replayMethodCall(HistogramRecord methodCall) {
        switch (methodCall.getRecordType()) {
            case HISTOGRAM_BOOLEAN:
                replayBooleanHistogram(methodCall);
                break;
            case HISTOGRAM_EXPONENTIAL:
                replayExponentialHistogram(methodCall);
                break;
            case HISTOGRAM_LINEAR:
                replayLinearHistogram(methodCall);
                break;
            case HISTOGRAM_SPARSE:
                replaySparseHistogram(methodCall);
                break;
            case USER_ACTION:
                replayUserAction(methodCall);
                break;
            default:
                Log.d(TAG, "Unrecognized record type");
        }
    }

    // Don't instantiate this class.
    private AwNonembeddedUmaReplayer() {}
}
