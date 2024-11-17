// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import androidx.annotation.Nullable;

import com.google.common.base.Strings;

import org.chromium.base.test.transit.ConditionStatus.Status;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;

/** Receives status updates and stores them as regions of equal updates. */
class StatusStore {
    private ArrayList<StatusRegion> mStatusRegions = new ArrayList<>();
    private StatusRegion mLastRegion;
    private boolean mAnyErrorsReported;
    private boolean mAnyMessages;

    void report(ConditionStatus status) {
        if (statusCanBeCollapsed(status)) {
            mLastRegion.reportUpdate(status.getTimestamp());
        } else {
            StatusRegion region = new StatusRegion(status);
            mStatusRegions.add(region);
            mLastRegion = region;
            if (status.isError()) {
                mAnyErrorsReported = true;
            }
            if (status.getMessage() != null) {
                mAnyMessages = true;
            }
        }
    }

    private boolean statusCanBeCollapsed(ConditionStatus status) {
        if (mLastRegion == null) {
            return false;
        }

        if (status.getStatus() != mLastRegion.mStatus) {
            return false;
        }

        return Objects.equals(status.getMessage(), mLastRegion.mMessage);
    }

    /**
     * @return all the {@link StatusRegion}s in chronological order.
     */
    List<StatusRegion> getStatusRegions() {
        return mStatusRegions;
    }

    boolean anyErrorsReported() {
        return mAnyErrorsReported;
    }

    boolean shouldPrintRegions() {
        return mAnyErrorsReported || mAnyMessages || mStatusRegions.size() > 2;
    }

    /** Represents one or more consecutive reports of the same status message. */
    static class StatusRegion {

        private @Status int mStatus;
        private long mFirstTimestamp;
        private long mLastTimestamp;
        private @Nullable String mMessage;
        private int mCount = 1;

        private StatusRegion(ConditionStatus firstStatus) {
            mStatus = firstStatus.getStatus();
            mMessage = firstStatus.getMessage();
            mFirstTimestamp = firstStatus.getTimestamp();
            mLastTimestamp = mFirstTimestamp;
        }

        private void reportUpdate(long timestamp) {
            mLastTimestamp = timestamp;
            mCount++;
        }

        String getLogString(long startTime) {
            if (mCount == 1) {
                return String.format(
                        "      %5dms (  1x): %s %s",
                        mFirstTimestamp - startTime,
                        getStatusPrefix(),
                        Strings.nullToEmpty(mMessage));
            } else {
                return String.format(
                        "%5d-%5dms (%3dx): %s %s",
                        mFirstTimestamp - startTime,
                        mLastTimestamp - startTime,
                        mCount,
                        getStatusPrefix(),
                        Strings.nullToEmpty(mMessage));
            }
        }

        private String getStatusPrefix() {
            return switch (mStatus) {
                case Status.FULFILLED -> "OK   |";
                case Status.NOT_FULFILLED -> "NO   |";
                case Status.ERROR -> "ERR  |";
                case Status.AWAITING -> "WAIT |";
                default -> throw new IllegalStateException("Unexpected value: " + mStatus);
            };
        }
    }
}
