// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.feed;

import androidx.annotation.Nullable;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;

/** Helper which is able to track a Scroll and aggregate them before sending the scroll events. */
public abstract class ScrollTracker {
    // onScroll events are very noisy, so we collate them together to avoid over-reporting scrolls.
    private static final long SCROLL_EVENT_COLLATE_MILLIS = 200L;

    @Nullable private ReportFunction mPostedReportFunction;

    // If |mScrollAmount| is non-zero and should be reported when ReportFunction runs.
    private boolean mReadyToReport;
    // How much scrolling is currently unreported. May be negative.
    private int mScrollAmount;

    public void onUnbind() {
        reportAndReset();
    }

    public void trackScroll(int dx, int dy) {
        if (dy == 0) {
            return;
        }

        if (mScrollAmount != 0 && (dy > 0) != (mScrollAmount > 0)) {
            // Scroll direction changing, report it now.
            reportAndReset();
        }

        mScrollAmount += dy;
        if (mPostedReportFunction == null) {
            mReadyToReport = true;
            mPostedReportFunction = new ReportFunction();
            PostTask.postDelayedTask(
                    TaskTraits.UI_DEFAULT, mPostedReportFunction, SCROLL_EVENT_COLLATE_MILLIS);
        } else {
            mReadyToReport = false;
        }
    }

    private void reportAndReset() {
        if (mScrollAmount != 0) {
            onScrollEvent(mScrollAmount);
            mScrollAmount = 0;
        }
        mReadyToReport = false;
    }

    protected abstract void onScrollEvent(int scrollAmount);

    private class ReportFunction implements Runnable {
        @Override
        public void run() {
            if (mReadyToReport) {
                reportAndReset();
                mPostedReportFunction = null;
            } else if (mScrollAmount != 0) {
                mReadyToReport = true;
                PostTask.postDelayedTask(
                        TaskTraits.UI_DEFAULT, mPostedReportFunction, SCROLL_EVENT_COLLATE_MILLIS);
            }
        }
    }
}
