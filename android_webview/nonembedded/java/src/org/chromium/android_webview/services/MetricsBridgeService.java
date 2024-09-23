// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.android_webview.services;

import android.app.Service;
import android.content.Intent;
import android.os.Binder;
import android.os.IBinder;
import android.os.Process;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import com.google.protobuf.InvalidProtocolBufferException;

import org.chromium.android_webview.common.services.IMetricsBridgeService;
import org.chromium.android_webview.proto.MetricsBridgeRecords.HistogramRecord;
import org.chromium.android_webview.proto.MetricsBridgeRecords.HistogramRecord.RecordType;
import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskRunner;
import org.chromium.base.task.TaskTraits;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.FutureTask;

/** Service that keeps record of UMA method calls in nonembedded WebView processes. */
public final class MetricsBridgeService extends Service {
    private static final String TAG = "MetricsBridgeService";

    // Max histograms this service will store, arbitrarily chosen
    private static final int MAX_HISTOGRAM_COUNT = 512;

    private static final String LOG_FILE_NAME = "webview_metrics_bridge_logs";

    private final File mLogFile;

    // Not guarded by a lock because it should only be accessed in a SequencedTaskRunner.
    private FileOutputStream mFileOutputStream;
    private List<byte[]> mRecordsList = new ArrayList<>();

    // To avoid any potential synchronization issues as well as avoid blocking the caller thread
    // (e.g when the caller is a thread from the same process.), we post all read/write operations
    // to be run serially using a SequencedTaskRunner instead of using a lock.
    private static final TaskRunner sSequencedTaskRunner =
            PostTask.createSequencedTaskRunner(TaskTraits.BEST_EFFORT_MAY_BLOCK);

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @VisibleForTesting
    @IntDef({
        ParsingLogResult.SUCCESS,
        ParsingLogResult.MALFORMED_PROTOBUF,
        ParsingLogResult.IO_EXCEPTION
    })
    public @interface ParsingLogResult {
        int SUCCESS = 0;
        int MALFORMED_PROTOBUF = 1;
        int IO_EXCEPTION = 2;
        int COUNT = 3;
    }

    // Adding a histogram record to list and not calling base.metrics.RecordHistogram to avoid the
    // service calling itself.
    private void logParsingLogResult(@ParsingLogResult int sample) {
        // Similar to calling RecordHistogram.recordEnumeratedHistogram(
        //        "Android.WebView.NonEmbeddedMetrics.ParsingLogResult", sample,
        //        ParsingLogResult.COUNT);
        HistogramRecord record =
                HistogramRecord.newBuilder()
                        .setRecordType(RecordType.HISTOGRAM_LINEAR)
                        .setHistogramName("Android.WebView.NonEmbeddedMetrics.ParsingLogResult")
                        .setSample(sample)
                        .setMin(1)
                        .setMax(ParsingLogResult.COUNT)
                        .setNumBuckets(ParsingLogResult.COUNT + 1)
                        .build();
        // Add to the in-memory list but never written to file to avoid filling up the record list
        // and file with redundant records. However, this means when this record is sent to embedded
        // WebView it represents the parsing result for the most recent service start only.
        mRecordsList.add(record.toByteArray());
    }

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @VisibleForTesting
    @IntDef({
        RetrieveMetricsTaskStatus.SUCCESS,
        RetrieveMetricsTaskStatus.EXECUTION_EXCEPTION,
        RetrieveMetricsTaskStatus.INTERRUPTED_EXCEPTION
    })
    public @interface RetrieveMetricsTaskStatus {
        int SUCCESS = 0;
        int EXECUTION_EXCEPTION = 1;
        int INTERRUPTED_EXCEPTION = 2;
        int COUNT = 3;
    }

    // Build a histogram record synchronously so it can be included in the batch of records sent to
    // the client instead of calling the base.metrics.RecordHistogram API (which is async and will
    // log in the next batch of records). This histogram captures errors that might happen when the
    // service is unable to send the current batch to the client. That's why this has to be added to
    // the current batch being sent.
    private static byte[] logRetrieveMetricsTaskStatus(@RetrieveMetricsTaskStatus int sample) {
        // Similar to calling RecordHistogram.recordEnumeratedHistogram(
        //        "Android.WebView.NonEmbeddedMetrics.RetrieveMetricsTaskStatus", sample,
        //        RetrieveMetricsTaskStatus.COUNT);
        HistogramRecord record =
                HistogramRecord.newBuilder()
                        .setRecordType(RecordType.HISTOGRAM_LINEAR)
                        .setHistogramName(
                                "Android.WebView.NonEmbeddedMetrics.RetrieveMetricsTaskStatus")
                        .setSample(sample)
                        .setMin(1)
                        .setMax(RetrieveMetricsTaskStatus.COUNT)
                        .setNumBuckets(ParsingLogResult.COUNT + 1)
                        .build();
        return record.toByteArray();
    }

    @Override
    public void onCreate() {
        // Restore saved histograms from disk.
        sSequencedTaskRunner.execute(
                () -> {
                    File file = getMetricsLogFile();
                    if (!file.exists()) return;
                    try (FileInputStream in = new FileInputStream(file)) {
                        HistogramRecord proto;
                        while ((proto = HistogramRecord.parseDelimitedFrom(in)) != null) {
                            // The proto message object isn't needed anymore, we will store its byte
                            // serialization.
                            mRecordsList.add(proto.toByteArray());
                        }
                        logParsingLogResult(ParsingLogResult.SUCCESS);
                    } catch (InvalidProtocolBufferException | IllegalStateException e) {
                        Log.e(TAG, "Malformed metrics log proto", e);
                        logParsingLogResult(ParsingLogResult.MALFORMED_PROTOBUF);
                        deleteMetricsLogFile();
                    } catch (IOException e) {
                        logParsingLogResult(ParsingLogResult.IO_EXCEPTION);
                        Log.e(TAG, "Failed reading proto log file", e);
                    }
                });
    }

    public MetricsBridgeService() {
        this(new File(PathUtils.getDataDirectory(), LOG_FILE_NAME));
    }

    @VisibleForTesting
    // Inject a logFile for testing.
    public MetricsBridgeService(File logFile) {
        mLogFile = logFile;
    }

    private final IMetricsBridgeService.Stub mBinder =
            new IMetricsBridgeService.Stub() {
                @Override
                public void recordMetrics(byte[] data) {
                    if (Binder.getCallingUid() != Process.myUid()) {
                        throw new SecurityException(
                                "recordMetrics() may only be called by non-embedded WebView"
                                        + " processes");
                    }
                    // If this is called within the same process, it will run on the caller thread,
                    // so we will always punt this to thread pool.
                    sSequencedTaskRunner.execute(
                            () -> {
                                // Make sure that we don't add records indefinitely in case of no
                                // embedded WebView connects to the service to retrieve and clear
                                // the records.
                                if (mRecordsList.size() >= MAX_HISTOGRAM_COUNT) {
                                    // TODO(crbug.com/40695441) add a histogram to log the
                                    // number of dropped histograms.
                                    Log.w(
                                            TAG,
                                            "retained records has reached the max capacity,"
                                                    + " dropping record");
                                    return;
                                }
                                try {
                                    // Parse data to make sure it's valid HistogramRecord byte data.
                                    HistogramRecord proto = HistogramRecord.parseFrom(data);
                                    mRecordsList.add(data);
                                    // Append the histogram record to log file.
                                    FileOutputStream out = getMetricsLogOutputStream();
                                    proto.writeDelimitedTo(out);
                                    // Flush the stream to make sure the bytes are written to file
                                    // in cases when the service isn't closed gracefully.
                                    out.flush();
                                } catch (InvalidProtocolBufferException e) {
                                    Log.e(TAG, "Malformed metrics log proto", e);
                                } catch (IOException e) {
                                    Log.e(TAG, "Failed to write to file", e);
                                }
                            });
                }

                @Override
                public List<byte[]> retrieveNonembeddedMetrics() {
                    FutureTask<List<byte[]>> retrieveFutureTask =
                            new FutureTask<>(
                                    () -> {
                                        List<byte[]> list = mRecordsList;
                                        mRecordsList = new ArrayList<>();
                                        deleteMetricsLogFile();
                                        return list;
                                    });
                    sSequencedTaskRunner.execute(retrieveFutureTask);
                    try {
                        return retrieveFutureTask.get();
                    } catch (ExecutionException e) {
                        Log.e(TAG, "error executing retrieveNonembeddedMetrics future task", e);
                    } catch (InterruptedException e) {
                        Log.e(TAG, "retrieveNonembeddedMetrics future task interrupted", e);
                    }
                    return new ArrayList<>();
                }
            };

    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }

    private File getMetricsLogFile() {
        return mLogFile;
    }

    private FileOutputStream getMetricsLogOutputStream() throws IOException {
        if (mFileOutputStream == null) {
            mFileOutputStream = new FileOutputStream(getMetricsLogFile(), /* append= */ true);
        }
        return mFileOutputStream;
    }

    private void closeMetricsLogOutputStream() {
        try {
            if (mFileOutputStream != null) {
                mFileOutputStream.close();
            }
        } catch (IOException e) {
            Log.e(TAG, "Couldn't close file output stream", e);
        } finally {
            mFileOutputStream = null;
        }
    }

    private boolean deleteMetricsLogFile() {
        closeMetricsLogOutputStream();
        return getMetricsLogFile().delete();
    }

    @Override
    public void onDestroy() {
        closeMetricsLogOutputStream();
    }

    /**
     * Add a FutureTask that can be used to block until all the tasks in the local {@code
     * sSequencedTaskRunner} are finished for testing.
     */
    @VisibleForTesting
    public FutureTask addTaskToBlock() {
        FutureTask<Object> blockTask = new FutureTask<Object>(() -> {}, new Object());
        sSequencedTaskRunner.execute(blockTask);
        return blockTask;
    }
}
