// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.historyreport;

import androidx.annotation.VisibleForTesting;

import java.io.PrintWriter;

/**
 * Defines contract which has to be fulfilled by data provider on native side.
 */
public interface SearchJniBridge {

    /**
     * Inits native side and registers data change observer.
     * Native initialization will be done on UI thread.
     */
    boolean init(DataChangeObserver observer);

    /**
     * Queries native side for delta file entries which will be served to local indexing service.
     * @param lastSeqNo which is a lower bound for seqno's of returned entries
     * @param limit of returned entries
     */
    DeltaFileEntry[] query(long lastSeqNo, int limit);

    /**
     * Trims delta file by dropping entries with seqno smaller and equal to seqNoLowerBound.
     * It returns highest seqno in delta file.
     */
    long trimDeltaFile(long seqNoLowerBound);

    /**
     * Queries native side for a batch of usage reports which will be sync'ed with local indexing
     * service.
     * @param batchSize intended number of usage reports in a batch.
     */
    UsageReport[] getUsageReportsBatch(int batchSize);

    /**
     * Removes usage reports from the internal buffer.
     */
    void removeUsageReports(UsageReport[] reports);

    /**
     * Clear the buffer of usage reports.
     */
    void clearUsageReports();

    /**
     * Adds all the historic visits to the usage report buffer.
     *
     * Should be done only once.
     * @return whether the visits were successfully added to the buffer.
     */
    boolean addHistoricVisitsToUsageReportsBuffer();

    /**
     * Observer on data changes.
     */
    public static interface DataChangeObserver {
        /**
         * Called when data has been changed.
         */
        void onDataChanged();
        /**
         * Called when data has been cleared.
         */
        void onDataCleared();
        /**
         * Called when usage reports can be reported to local indexing service.
         */
        void startReportingTask();
        /**
         * Called when usage reports can't be reported to local indexing service any more.
         */
        void stopReportingTask();
    }

    @VisibleForTesting
    boolean isStartedForTest();

    void dump(PrintWriter writer);
}
