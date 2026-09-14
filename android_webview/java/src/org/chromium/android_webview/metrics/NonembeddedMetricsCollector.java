// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.metrics;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.IBinder;

import androidx.annotation.IntDef;

import com.google.protobuf.InvalidProtocolBufferException;

import org.chromium.android_webview.AwBrowserProcess;
import org.chromium.android_webview.ManifestMetadataUtil;
import org.chromium.android_webview.common.services.IMetricsBridgeService;
import org.chromium.android_webview.common.services.ServiceConnectionDelayRecorder;
import org.chromium.android_webview.common.services.ServiceNames;
import org.chromium.android_webview.proto.MetricsBridgeRecords.HistogramRecord;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.TimeUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;

import java.util.List;
import java.util.concurrent.TimeUnit;

/** Handles collecting and transmitting UMA metrics recorded by non-embedded WebView services. */
@NullMarked
public final class NonembeddedMetricsCollector {
    private static final String TAG = "NonembedMetrics";

    private static final int MINUTES_PER_DAY =
            (int) TimeUnit.SECONDS.toMinutes(TimeUtils.SECONDS_PER_DAY);

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        TransmissionResult.SUCCESS,
        TransmissionResult.MALFORMED_PROTOBUF,
        TransmissionResult.REMOTE_EXCEPTION
    })
    private @interface TransmissionResult {
        int SUCCESS = 0;
        int MALFORMED_PROTOBUF = 1;
        int REMOTE_EXCEPTION = 2;
        int COUNT = 3;
    }

    private static void logTransmissionResult(@TransmissionResult int sample) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.WebView.NonEmbeddedMetrics.TransmissionResult",
                sample,
                TransmissionResult.COUNT);
    }

    /**
     * Record very long times UMA histogram up to 4 days.
     *
     * @param name histogram name.
     * @param time time sample in millis.
     */
    private static void recordVeryLongTimesHistogram(String name, long time) {
        long timeMins = TimeUnit.MILLISECONDS.toMinutes(time);
        int sample;
        // Safely convert to int to avoid positive or negative overflow.
        if (timeMins > Integer.MAX_VALUE) {
            sample = Integer.MAX_VALUE;
        } else if (timeMins < Integer.MIN_VALUE) {
            sample = Integer.MIN_VALUE;
        } else {
            sample = (int) timeMins;
        }
        RecordHistogram.recordCustomCountHistogram(name, sample, 1, 4 * MINUTES_PER_DAY, 50);
    }

    /**
     * Connect to {@link org.chromium.android_webview.services.MetricsBridgeService} to retrieve any
     * recorded UMA metrics from nonembedded WebView services and transmit them back using UMA APIs.
     */
    public static void collectNonembeddedMetrics() {
        if (ManifestMetadataUtil.isAppOptedOutFromMetricsCollection()) {
            Log.d(TAG, "App opted out from metrics collection, not connecting to metrics service");
            return;
        }

        final Intent intent = new Intent();
        intent.setClassName(
                AwBrowserProcess.getWebViewPackageName(), ServiceNames.METRICS_BRIDGE_SERVICE);

        ServiceConnectionDelayRecorder connection =
                new ServiceConnectionDelayRecorder() {
                    private boolean mHasConnected;

                    @Override
                    public void onServiceConnectedImpl(ComponentName className, IBinder service) {
                        if (mHasConnected) return;
                        mHasConnected = true;
                        // onServiceConnected is called on the UI thread, so punt this back to the
                        // background thread.
                        PostTask.postTask(
                                TaskTraits.BEST_EFFORT,
                                () -> {
                                    sendMetricsToService(service);
                                    ContextUtils.getApplicationContext().unbindService(this);
                                });
                    }

                    @Override
                    public void onServiceDisconnected(ComponentName className) {}
                };

        Context appContext = ContextUtils.getApplicationContext();
        if (!connection.bind(appContext, intent, Context.BIND_AUTO_CREATE)) {
            Log.d(TAG, "Could not bind to MetricsBridgeService %s", intent);
        }
    }

    // AIDL returns a raw List because List<byte[]> is not a supported AIDL type.
    @SuppressWarnings("unchecked")
    private static void sendMetricsToService(IBinder service) {
        try {
            IMetricsBridgeService metricsService = IMetricsBridgeService.Stub.asInterface(service);

            List<byte[]> data = metricsService.retrieveNonembeddedMetrics();
            RecordHistogram.recordCount1000Histogram(
                    "Android.WebView.NonEmbeddedMetrics.NumHistograms", data.size());
            long systemTime = System.currentTimeMillis();
            for (byte[] recordData : data) {
                HistogramRecord record = HistogramRecord.parseFrom(recordData);
                AwNonembeddedUmaReplayer.replayMethodCall(record);
                if (record.hasMetadata()) {
                    long timeRecorded = record.getMetadata().getTimeRecorded();
                    recordVeryLongTimesHistogram(
                            "Android.WebView.NonEmbeddedMetrics.HistogramRecordAge",
                            systemTime - timeRecorded);
                }
            }
            logTransmissionResult(TransmissionResult.SUCCESS);
        } catch (InvalidProtocolBufferException e) {
            Log.d(TAG, "Malformed metrics log proto", e);
            logTransmissionResult(TransmissionResult.MALFORMED_PROTOBUF);
        } catch (Exception e) {
            // RemoteException, IllegalArgumentException
            // (https://crbug.com/1403976)
            Log.d(TAG, "Remote Exception in MetricsBridgeService#retrieveMetrics", e);
            logTransmissionResult(TransmissionResult.REMOTE_EXCEPTION);
        }
    }

    private NonembeddedMetricsCollector() {}
}
