// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.historyreport;

import static org.chromium.base.ThreadUtils.assertOnBackgroundThread;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.task.PostTask;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.io.PrintWriter;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Bridge class for calls between Java and C++.
 */
@JNINamespace("history_report")
public class HistoryReportJniBridge implements SearchJniBridge {
    private static final String TAG = "historyreport";

    private long mNativeHistoryReportJniBridge;
    private DataChangeObserver mDataChangeObserver;
    private final AtomicBoolean mStarted = new AtomicBoolean();

    @Override
    public boolean init(DataChangeObserver observer) {
        // This is called in the deferred task, so we couldn't assertOnBackgroundThread();
        assert mDataChangeObserver == null || mDataChangeObserver == observer;
        if (observer == null) return false;
        if (mNativeHistoryReportJniBridge != 0) return true;
        mDataChangeObserver = observer;
        PostTask.runSynchronously(UiThreadTaskTraits.DEFAULT, () -> {
            mNativeHistoryReportJniBridge =
                    HistoryReportJniBridgeJni.get().init(HistoryReportJniBridge.this);
        });
        if (mNativeHistoryReportJniBridge == 0) {
            Log.w(TAG, "JNI bridge initialization unsuccessful.");
            return false;
        }

        Log.d(TAG, "JNI bridge initialization successful.");
        mStarted.set(true);
        return true;
    }

    private boolean isInitialized() {
        return mNativeHistoryReportJniBridge != 0 && mDataChangeObserver != null && mStarted.get();
    }

    @VisibleForTesting
    @Override
    public boolean isStartedForTest() {
        return mStarted.get();
    }

    @Override
    public DeltaFileEntry[] query(long lastSeqNo, int limit) {
        assertOnBackgroundThread();
        if (!isInitialized()) {
            Log.w(TAG, "query when JNI bridge not initialized");
            return new DeltaFileEntry[0];
        }
        Log.d(TAG, "query %d %d", lastSeqNo, limit);
        DeltaFileEntry[] result = HistoryReportJniBridgeJni.get().query(
                mNativeHistoryReportJniBridge, HistoryReportJniBridge.this, lastSeqNo, limit);
        return result;
    }

    @Override
    public long trimDeltaFile(long seqNoLowerBound) {
        assertOnBackgroundThread();
        if (!isInitialized()) {
            Log.w(TAG, "trimDeltaFile when JNI bridge not initialized");
            return -1;
        }
        Log.d(TAG, "trimDeltaFile %d", seqNoLowerBound);
        return HistoryReportJniBridgeJni.get().trimDeltaFile(
                mNativeHistoryReportJniBridge, HistoryReportJniBridge.this, seqNoLowerBound);
    }

    @Override
    public UsageReport[] getUsageReportsBatch(int batchSize) {
        assertOnBackgroundThread();
        if (!isInitialized()) {
            Log.w(TAG, "getUsageReportsBatch when JNI bridge not initialized");
            return new UsageReport[0];
        }
        Log.d(TAG, "getUsageReportsBatch %d", batchSize);
        return HistoryReportJniBridgeJni.get().getUsageReportsBatch(
                mNativeHistoryReportJniBridge, HistoryReportJniBridge.this, batchSize);
    }

    @Override
    public void removeUsageReports(UsageReport[] reports) {
        assertOnBackgroundThread();
        if (!isInitialized()) {
            Log.w(TAG, "removeUsageReports when JNI bridge not initialized");
            return;
        }
        String[] reportIds = new String[reports.length];
        for (int i = 0; i < reports.length; ++i) {
            reportIds[i] = reports[i].reportId;
        }
        HistoryReportJniBridgeJni.get().removeUsageReports(
                mNativeHistoryReportJniBridge, HistoryReportJniBridge.this, reportIds);
    }

    @Override
    public void clearUsageReports() {
        assertOnBackgroundThread();
        if (!isInitialized()) {
            Log.w(TAG, "clearUsageReports when JNI bridge not initialized");
            return;
        }
        HistoryReportJniBridgeJni.get().clearUsageReports(
                mNativeHistoryReportJniBridge, HistoryReportJniBridge.this);
    }

    @Override
    public boolean addHistoricVisitsToUsageReportsBuffer() {
        assertOnBackgroundThread();
        if (!isInitialized()) {
            Log.w(TAG, "addHistoricVisitsToUsageReportsBuffer when JNI bridge not initialized");
            return false;
        }
        return HistoryReportJniBridgeJni.get().addHistoricVisitsToUsageReportsBuffer(
                mNativeHistoryReportJniBridge, HistoryReportJniBridge.this);
    }

    @Override
    public void dump(PrintWriter writer) {
        writer.append("\nHistoryReportJniBridge [").append("started: " + mStarted.get())
                .append(", initialized: " + isInitialized());
        if (isInitialized()) {
            writer.append(", "
                    + HistoryReportJniBridgeJni.get().dump(
                            mNativeHistoryReportJniBridge, HistoryReportJniBridge.this));
        }
        writer.append("]");
    }

    @CalledByNative
    private static DeltaFileEntry[] createDeltaFileEntriesArray(int size) {
        return new DeltaFileEntry[size];
    }

    @CalledByNative
    private static void setDeltaFileEntry(DeltaFileEntry[] entries, int position, long seqNo,
            String type, String id, String url, int score, String title, String indexedUrl) {
        entries[position] = new DeltaFileEntry(seqNo, type, id, url, score, title, indexedUrl);
    }

    @CalledByNative
    private static UsageReport[] createUsageReportsArray(int size) {
        return new UsageReport[size];
    }

    @CalledByNative
    private static void setUsageReport(UsageReport[] reports, int position, String reportId,
            String pageId, long timestampMs, boolean typedVisit) {
        reports[position] = new UsageReport(reportId, pageId, timestampMs, typedVisit);
    }

    @CalledByNative
    private void onDataChanged() {
        Log.d(TAG, "onDataChanged");
        mDataChangeObserver.onDataChanged();
    }

    @CalledByNative
    private void onDataCleared() {
        Log.d(TAG, "onDataCleared");
        mDataChangeObserver.onDataCleared();
    }

    @CalledByNative
    private void startReportingTask() {
        Log.d(TAG, "startReportingTask");
        mDataChangeObserver.startReportingTask();
    }

    @CalledByNative
    private void stopReportingTask() {
        Log.d(TAG, "stopReportingTask");
        mDataChangeObserver.stopReportingTask();
    }

    @NativeMethods
    interface Natives {
        long init(HistoryReportJniBridge caller);
        long trimDeltaFile(long nativeHistoryReportJniBridge, HistoryReportJniBridge caller,
                long seqNoLowerBound);
        DeltaFileEntry[] query(long nativeHistoryReportJniBridge, HistoryReportJniBridge caller,
                long lastSeqNo, int limit);
        UsageReport[] getUsageReportsBatch(
                long nativeHistoryReportJniBridge, HistoryReportJniBridge caller, int batchSize);
        void removeUsageReports(long nativeHistoryReportJniBridge, HistoryReportJniBridge caller,
                String[] reportIds);
        void clearUsageReports(long nativeHistoryReportJniBridge, HistoryReportJniBridge caller);
        boolean addHistoricVisitsToUsageReportsBuffer(
                long nativeHistoryReportJniBridge, HistoryReportJniBridge caller);
        String dump(long nativeHistoryReportJniBridge, HistoryReportJniBridge caller);
    }
}
