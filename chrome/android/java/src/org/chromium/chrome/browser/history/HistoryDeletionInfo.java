// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;

/**
 * Android wrapper of the native history::DeletionInfo class. Any class that uses this needs to
 * register a {@link HistoryDeletionBridge.Observer} on {@Link HistoryDeletionBridge} to listen for
 * the native signals that produce this signal.
 */
public class HistoryDeletionInfo {
    private final long mHistoryDeletionInfoPtr;

    @CalledByNative
    private static HistoryDeletionInfo create(long historyDeletionInfoPtr) {
        return new HistoryDeletionInfo(historyDeletionInfoPtr);
    }

    HistoryDeletionInfo(long historyDeletionInfoPtr) {
        mHistoryDeletionInfoPtr = historyDeletionInfoPtr;
    }

    /**
     * @return An array of URLs that were deleted.
     */
    public String[] getDeletedURLs() {
        return HistoryDeletionInfoJni.get().getDeletedURLs(mHistoryDeletionInfoPtr);
    }

    /**
     * @return True if the time range is valid.
     */
    public boolean isTimeRangeValid() {
        return HistoryDeletionInfoJni.get().isTimeRangeValid(mHistoryDeletionInfoPtr);
    }

    /**
     * @return True if the time range is for all time.
     */
    public boolean isTimeRangeForAllTime() {
        return HistoryDeletionInfoJni.get().isTimeRangeForAllTime(mHistoryDeletionInfoPtr);
    }

    /**
     * @return The beginning of the time range if the time range is valid.
     */
    public long getTimeRangeBegin() {
        return HistoryDeletionInfoJni.get().getTimeRangeBegin(mHistoryDeletionInfoPtr);
    }

    /**
     * @return The end of the time range if the time range is valid.
     */
    public long getTimeRangeEnd() {
        return HistoryDeletionInfoJni.get().getTimeRangeBegin(mHistoryDeletionInfoPtr);
    }

    @NativeMethods
    interface Natives {
        String[] getDeletedURLs(long historyDeletionInfoPtr);
        boolean isTimeRangeValid(long historyDeletionInfoPtr);
        boolean isTimeRangeForAllTime(long historyDeletionInfoPtr);
        long getTimeRangeBegin(long historyDeletionInfoPtr);
        long getTimeRangeEnd(long historyDeletionInfoPtr);
    }
}
