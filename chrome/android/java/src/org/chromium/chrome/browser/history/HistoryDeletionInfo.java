// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;

/**
 * Android wrapper of the native history::DeletionInfo class. Any class that uses this needs to
 * register a {@link HistoryDeletionBridge.Observer} on {@link HistoryDeletionBridge} to listen for
 * the native signals that produce this signal.
 */
@NullMarked
public class HistoryDeletionInfo {
    private final String[] mDeletedURLs;
    private final boolean mIsTimeRangeValid;
    private final boolean mIsTimeRangeForAllTime;

    @CalledByNative
    private static HistoryDeletionInfo create(
            String[] deletedURLs, boolean isTimeRangeValid, boolean isTimeRangeForAllTime) {
        return new HistoryDeletionInfo(deletedURLs, isTimeRangeValid, isTimeRangeForAllTime);
    }

    HistoryDeletionInfo(
            String[] deletedURLs, boolean isTimeRangeValid, boolean isTimeRangeForAllTime) {
        mDeletedURLs = deletedURLs;
        mIsTimeRangeValid = isTimeRangeValid;
        mIsTimeRangeForAllTime = isTimeRangeForAllTime;
    }

    /**
     * @return An array of URLs that were deleted.
     */
    public String[] getDeletedURLs() {
        return mDeletedURLs;
    }

    /**
     * @return True if the time range is valid.
     */
    public boolean isTimeRangeValid() {
        return mIsTimeRangeValid;
    }

    /**
     * @return True if the time range is for all time.
     */
    public boolean isTimeRangeForAllTime() {
        return mIsTimeRangeForAllTime;
    }
}
