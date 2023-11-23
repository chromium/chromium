// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.android_webview.nonembedded;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Binder;
import android.os.IBinder;
import android.os.RemoteException;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.android_webview.common.services.IMetricsBridgeService;
import org.chromium.android_webview.common.services.ServiceHelper;
import org.chromium.android_webview.common.services.ServiceNames;
import org.chromium.android_webview.proto.MetricsBridgeRecords.HistogramRecord;
import org.chromium.android_webview.proto.MetricsBridgeRecords.HistogramRecord.Metadata;
import org.chromium.android_webview.proto.MetricsBridgeRecords.HistogramRecord.RecordType;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.HistogramBucket;
import org.chromium.base.metrics.UmaRecorder;

import java.util.ArrayList;
import java.util.List;

import javax.annotation.concurrent.GuardedBy;

/**
 * {@link UmaRecorder} for nonembedded WebView processes.
 * Can be used as a delegate in {@link org.chromium.base.metrics.UmaRecorderHolder}. This may only
 * be called from non-embedded WebView processes, such as developer UI or Services.
 */
public class AwNonembeddedUmaRecorder implements UmaRecorder {
    private static final String TAG = "AwNonembedUmaRecord";

    // Arbitrary limit to avoid adding records indefinitely if there is a problem connecting to the
    // service.
    @VisibleForTesting public static final int MAX_PENDING_RECORDS_COUNT = 512;

    private final RecordingDelegate mRecordingDelegate;

    @VisibleForTesting
    public AwNonembeddedUmaRecorder(RecordingDelegate delegate) {
        mRecordingDelegate = delegate;
    }

    public AwNonembeddedUmaRecorder() {
        this(new RecordingDelegate());
    }

    /** Records a single sample of a boolean histogram. */
    @Override
    public void recordBooleanHistogram(String name, boolean sample) {
        HistogramRecord.Builder builder = HistogramRecord.newBuilder();
        builder.setRecordType(RecordType.HISTOGRAM_BOOLEAN);
        builder.setHistogramName(name);
        builder.setSample(sample ? 1 : 0);

        recordHistogram(builder.build());
    }

    /**
     * Records a single sample of a histogram with exponentially scaled buckets. See
     * {@link
     * https://chromium.googlesource.com/chromium/src.git/+/HEAD/tools/metrics/histograms/README.md#count-histograms}
     * <p>
     * This is the default histogram type used by "counts", "times" and "memory" histograms in
     * {@link org.chromium.base.metrics.RecordHistogram}
     *
     * @param min the smallest value recorded in the first bucket; should be greater than zero.
     * @param max the smallest value recorded in the overflow bucket.
     * @param numBuckets number of histogram buckets: Two buckets are used for underflow and
     *         overflow, and the remaining buckets cover the range {@code [min, max)}; {@code
     *         numBuckets} should be {@code 100} or less.
     */
    @Override
    public void recordExponentialHistogram(
            String name, int sample, int min, int max, int numBuckets) {
        HistogramRecord.Builder builder = HistogramRecord.newBuilder();
        builder.setRecordType(RecordType.HISTOGRAM_EXPONENTIAL);
        builder.setHistogramName(name);
        builder.setSample(sample);
        builder.setMin(min);
        builder.setMax(max);
        builder.setNumBuckets(numBuckets);

        recordHistogram(builder.build());
    }

    /**
     * Records a single sample of a histogram with evenly spaced buckets. See
     * {@link
     * https://chromium.googlesource.com/chromium/src.git/+/HEAD/tools/metrics/histograms/README.md#percentage-or-ratio-histograms}
     * <p>
     * This histogram type is best suited for recording enums, percentages and ratios.
     *
     * @param min the smallest value recorded in the first bucket; should be equal to one, but will
     *         work with values greater than zero.
     * @param max the smallest value recorded in the overflow bucket.
     * @param numBuckets number of histogram buckets: Two buckets are used for underflow and
     *         overflow, and the remaining buckets evenly cover the range {@code [min, max)}; {@code
     *         numBuckets} should be {@code 100} or less.
     */
    @Override
    public void recordLinearHistogram(String name, int sample, int min, int max, int numBuckets) {
        HistogramRecord.Builder builder = HistogramRecord.newBuilder();
        builder.setRecordType(RecordType.HISTOGRAM_LINEAR);
        builder.setHistogramName(name);
        builder.setSample(sample);
        builder.setMin(min);
        builder.setMax(max);
        builder.setNumBuckets(numBuckets);

        recordHistogram(builder.build());
    }

    /**
     * Records a single sample of a sparse histogram. See
     * {@link
     * https://chromium.googlesource.com/chromium/src.git/+/HEAD/tools/metrics/histograms/README.md#when-to-use-sparse-histograms}
     */
    @Override
    public void recordSparseHistogram(String name, int sample) {
        HistogramRecord.Builder builder = HistogramRecord.newBuilder();
        builder.setRecordType(RecordType.HISTOGRAM_SPARSE);
        builder.setHistogramName(name);
        builder.setSample(sample);

        recordHistogram(builder.build());
    }

    /**
     * Records a user action. Action names must be documented in {@code actions.xml}. See {@link
     * https://source.chromium.org/chromium/chromium/src/+/main:tools/metrics/actions/README.md}
     *
     * @param name Name of the user action.
     * @param elapsedRealtimeMillis Value of {@link android.os.SystemClock.elapsedRealtime()} when
     *         the action was observed.
     */
    @Override
    public void recordUserAction(String name, long elapsedRealtimeMillis) {
        HistogramRecord record =
                HistogramRecord.newBuilder()
                        .setRecordType(RecordType.USER_ACTION)
                        .setHistogramName(name)
                        .setElapsedRealtimeMillis(elapsedRealtimeMillis)
                        .build();

        recordHistogram(record);
    }

    @Override
    public int getHistogramValueCountForTesting(String name, int sample) {
        throw new UnsupportedOperationException();
    }

    @Override
    public int getHistogramTotalCountForTesting(String name) {
        throw new UnsupportedOperationException();
    }

    @Override
    public List<HistogramBucket> getHistogramSamplesForTesting(String name) {
        throw new UnsupportedOperationException();
    }

    @Override
    public void addUserActionCallbackForTesting(Callback<String> callback) {
        throw new UnsupportedOperationException();
    }

    @Override
    public void removeUserActionCallbackForTesting(Callback<String> callback) {
        throw new UnsupportedOperationException();
    }

    private final Object mLock = new Object();

    // Service stub object
    @GuardedBy("mLock")
    @Nullable
    private IMetricsBridgeService mServiceStub;

    // List of HistogramRecords that are pending to be sent to the service because the connection
    // isn't ready yet.
    @GuardedBy("mLock")
    private final List<HistogramRecord> mPendingRecordsList = new ArrayList<>();

    private final ServiceConnection mServiceConnection =
            new ServiceConnection() {
                @Override
                public void onServiceConnected(ComponentName className, IBinder service) {
                    synchronized (mLock) {
                        mServiceStub = IMetricsBridgeService.Stub.asInterface(service);
                        for (HistogramRecord record : mPendingRecordsList) {
                            sendToServiceLocked(record);
                        }
                        mPendingRecordsList.clear();
                    }
                }

                @Override
                public void onServiceDisconnected(ComponentName className) {
                    synchronized (mLock) {
                        mServiceStub = null;
                    }
                }
            };

    /**
     * Send a record to the metrics service, assumes that {@code mLock} is held by the current
     * thread.
     */
    @GuardedBy("mLock")
    private void sendToServiceLocked(HistogramRecord record) {
        // Clear the calling identity for cases when this is called locally in the same process.
        long token = Binder.clearCallingIdentity();
        try {
            // We are not punting this to a background thread since the cost of IPC itself
            // should be relatively cheap, and the remote method does its work
            // asynchronously.
            mServiceStub.recordMetrics(record.toByteArray());
        } catch (RemoteException e) {
            Log.e(TAG, "Remote Exception calling IMetricsBridgeService#recordMetrics", e);
        } finally {
            Binder.restoreCallingIdentity(token);
        }
    }

    // Is app context bound to the MetricsBridgeService.
    @GuardedBy("mLock")
    private boolean mIsBound;

    /**
     * Bind the service only once and keep the service binding for the lifetime of the app process.
     * Assumes that {@code mLock} is held by the current thread.
     *
     * We never unbind because it's fine to keep the service alive while the app is running, since
     * it's most likely that there will be consistent stream of histograms while the app/process is
     * running.
     */
    @GuardedBy("mLock")
    private void maybeBindServiceLocked() {
        if (mIsBound) return;

        final Context appContext = ContextUtils.getApplicationContext();
        final Intent intent = new Intent();
        intent.setClassName(appContext, mRecordingDelegate.getServiceName());
        mIsBound =
                ServiceHelper.bindService(
                        appContext, intent, mServiceConnection, Context.BIND_AUTO_CREATE);
        if (!mIsBound) {
            Log.w(TAG, "Could not bind to MetricsBridgeService " + intent);
        }
    }

    /**
     * Record a histogram by sending it to metrics service or add it to a pending list if the
     * connection isn't ready yet. Bind the service only once on the first record to arrive.
     */
    private void recordHistogram(HistogramRecord record) {
        record = mRecordingDelegate.addMetadata(record);

        synchronized (mLock) {
            if (mServiceStub != null) {
                sendToServiceLocked(record);
                return;
            }

            maybeBindServiceLocked();
            if (mPendingRecordsList.size() < MAX_PENDING_RECORDS_COUNT) {
                mPendingRecordsList.add(record);
            } else {
                Log.w(TAG, "Number of pending records has reached max capacity, dropping record");
            }
        }
    }

    /** A delegate class that allows customizing some actions for testing. */
    @VisibleForTesting
    public static class RecordingDelegate {
        /**
         * @return metrics service name that the Recorder will attempt connecting to it to record
         *         metrics.
         */
        public String getServiceName() {
            return ServiceNames.METRICS_BRIDGE_SERVICE;
        }

        /**
         * Add {@link Metadata} to {@link HistogramRecord} class.
         * Metadata consists of: the time when the histogram is received by the recorder.
         *
         * @return {@code record} after adding the metadata to it.
         */
        public HistogramRecord addMetadata(HistogramRecord record) {
            long millis = System.currentTimeMillis();
            Metadata metadata = Metadata.newBuilder().setTimeRecorded(millis).build();
            return record.toBuilder().setMetadata(metadata).build();
        }
    }
}
