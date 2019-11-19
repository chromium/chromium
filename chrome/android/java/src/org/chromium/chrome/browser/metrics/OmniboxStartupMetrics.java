// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import android.os.SystemClock;

import androidx.annotation.IntDef;

import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.WebContents;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Tracks the startup state of chrome at the time of first url bar focus.
 */
public class OmniboxStartupMetrics {
    // Used to record the UMA histogram MobileStartup.ToolbarFirstFocusStartupState.<activity name>.
    // Since these values are persisted to logs, they should never be renumbered nor reused.
    @IntDef({StartupState.PRE_NATIVE_INITIALIZATION, StartupState.POST_NATIVE_INITIALIZATION,
            StartupState.POST_FIRST_MEANINGFUL_PAINT, StartupState.POST_FIRST_PAGELOAD_FINISHED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface StartupState {
        // This is the state of startup before native has been loaded
        int PRE_NATIVE_INITIALIZATION = 0;
        // This is the state of startup after native has finished loaded but before the first
        // meaningful paint.
        int POST_NATIVE_INITIALIZATION = 1;
        // This is after the first meaningful paint but before the first page finished loading
        // completely
        int POST_FIRST_MEANINGFUL_PAINT = 2;
        // This is after the first page has been loaded completely.
        int POST_FIRST_PAGELOAD_FINISHED = 3;

        int NUM_ENTRIES = 4;
    }

    private static final int MIN_FOCUS_TIME_FOR_UMA_HISTOGRAM_MS = 1000;
    private static final int MAX_FOCUS_TIME_FOR_UMA_HISTOGRAM_MS = 30000;

    private final ChromeActivity mActivity;
    private @StartupState int mCurrentStartupState = StartupState.PRE_NATIVE_INITIALIZATION;
    // Startup state at the time the url bar is focused.
    private @StartupState int mUrlBarFocusedStartupState;
    // Track if the url bar has been focused since startup.
    private long mUrlBarFirstFocusedTime = -1;
    private TabModelSelectorTabObserver mTabModelSelectorTabObserver;
    private PageLoadMetrics.Observer mPageLoadMetricsObserver;
    private boolean mHistogramsRecorded;

    public OmniboxStartupMetrics(ChromeActivity activity) {
        mActivity = activity;
        registerObservers();
        BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER)
                .addStartupCompletedObserver(new BrowserStartupController.StartupCallback() {
                    @Override
                    public void onSuccess() {
                        onNativeLibraryLoaded();
                    }

                    @Override
                    public void onFailure() {}
                });
    }

    private void registerObservers() {
        mTabModelSelectorTabObserver =
                new TabModelSelectorTabObserver(mActivity.getTabModelSelector()) {
                    @Override
                    public void onPageLoadFinished(Tab tab, String url) {
                        updateStartupState(StartupState.POST_FIRST_PAGELOAD_FINISHED);
                    }
                };
        mPageLoadMetricsObserver = new PageLoadMetrics.Observer() {
            @Override
            public void onFirstMeaningfulPaint(WebContents webContents, long navigationId,
                    long navigationStartTick, long firstContentfulPaintMs) {
                if (mActivity.getActivityTab() != null
                        && mActivity.getActivityTab().getWebContents() != null
                        && mActivity.getActivityTab().getWebContents().equals(webContents)) {
                    updateStartupState(StartupState.POST_FIRST_MEANINGFUL_PAINT);
                }
            }
        };
        PageLoadMetrics.addObserver(mPageLoadMetricsObserver);
    }

    private void updateStartupState(@StartupState int newState) {
        // Should not go backwards in startup state
        if (newState < mCurrentStartupState) return;
        mCurrentStartupState = newState;
    }

    /**
     * Remove observers. This may be called multiple times (it is called when the histogram is
     * recorded since we only record once per startup, but also on owner.destroy)
     */
    public void destroy() {
        if (mTabModelSelectorTabObserver != null) {
            mTabModelSelectorTabObserver.destroy();
            mTabModelSelectorTabObserver = null;
        }

        if (mPageLoadMetricsObserver != null) {
            PageLoadMetrics.removeObserver(mPageLoadMetricsObserver);
            mPageLoadMetricsObserver = null;
        }
    }

    /**
     * Called when the url bar is focused, all calls after the first are ignored.
     */
    public void onUrlBarFocused() {
        if (mUrlBarFirstFocusedTime == -1) {
            mUrlBarFocusedStartupState = mCurrentStartupState;
            mUrlBarFirstFocusedTime = SystemClock.elapsedRealtime();
        }
    }

    /**
     * Called when the native library is loaded.
     */
    private void onNativeLibraryLoaded() {
        updateStartupState(StartupState.POST_NATIVE_INITIALIZATION);
    }

    /**
     * Called outside the critical path to record the actual UMA histogram.
     */
    public void maybeRecordHistograms() {
        if (mUrlBarFirstFocusedTime != -1 && !mHistogramsRecorded) {
            mHistogramsRecorded = true;
            String activityName = mActivity.getClass().getSimpleName();
            RecordHistogram.recordEnumeratedHistogram(
                    "MobileStartup.ToolbarFirstFocusStartupState." + activityName,
                    mUrlBarFocusedStartupState, StartupState.NUM_ENTRIES);

            RecordHistogram.recordCustomTimesHistogram(
                    "MobileStartup.ToolbarFirstFocusTime2." + activityName,
                    mUrlBarFirstFocusedTime - mActivity.getOnCreateTimestampMs(),
                    MIN_FOCUS_TIME_FOR_UMA_HISTOGRAM_MS, MAX_FOCUS_TIME_FOR_UMA_HISTOGRAM_MS, 50);
        }
        destroy();
    }
}
