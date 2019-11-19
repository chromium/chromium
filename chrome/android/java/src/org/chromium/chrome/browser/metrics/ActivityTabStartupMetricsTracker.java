// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import android.os.SystemClock;

import org.chromium.base.ObservableSupplier;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;

/**
 * Tracks the first navigation and first contentful paint events for a tab within an activity during
 * startup.
 */
public class ActivityTabStartupMetricsTracker {
    private final long mActivityStartTimeMs;

    // Event duration recorded from the |mActivityStartTimeMs|.
    private long mFirstCommitTimeMs;
    private String mHistogramSuffix;
    private TabModelSelectorTabObserver mTabModelSelectorTabObserver;
    private PageLoadMetrics.Observer mPageLoadMetricsObserver;
    private boolean mShouldTrackStartupMetrics;

    public ActivityTabStartupMetricsTracker(
            ObservableSupplier<TabModelSelector> tabModelSelectorSupplier) {
        mActivityStartTimeMs = SystemClock.uptimeMillis();
        tabModelSelectorSupplier.addObserver((selector) -> registerObservers(selector));
    }

    private void registerObservers(TabModelSelector tabModelSelector) {
        if (!mShouldTrackStartupMetrics) return;
        mTabModelSelectorTabObserver =
                new TabModelSelectorTabObserver(tabModelSelector) {
                    private boolean mIsFirstPageLoadStart = true;

                    @Override
                    public void onPageLoadStarted(Tab tab, String url) {
                        // Discard startup navigation measurements when the user interfered and
                        // started the 2nd navigation (in activity lifetime) in parallel.
                        if (!mIsFirstPageLoadStart) {
                            mShouldTrackStartupMetrics = false;
                        } else {
                            mIsFirstPageLoadStart = false;
                        }
                    }

                    @Override
                    public void onDidFinishNavigation(Tab tab, NavigationHandle navigation) {
                        boolean isTrackedPage = navigation.hasCommitted()
                                && navigation.isInMainFrame() && !navigation.isErrorPage()
                                && !navigation.isSameDocument()
                                && !navigation.isFragmentNavigation()
                                && UrlUtilities.isHttpOrHttps(navigation.getUrl());
                        registerFinishNavigation(isTrackedPage);
                    }
                };
        mPageLoadMetricsObserver = new PageLoadMetrics.Observer() {
            private static final long NO_NAVIGATION_ID = -1;

            private long mNavigationId = NO_NAVIGATION_ID;
            private boolean mShouldRecordHistograms;

            @Override
            public void onNewNavigation(WebContents webContents, long navigationId,
                    boolean isFirstNavigationInWebContents) {
                if (mNavigationId != NO_NAVIGATION_ID) return;

                mNavigationId = navigationId;
                mShouldRecordHistograms = mShouldTrackStartupMetrics;
            }

            @Override
            public void onFirstContentfulPaint(WebContents webContents, long navigationId,
                    long navigationStartTick, long firstContentfulPaintMs) {
                if (navigationId != mNavigationId || !mShouldRecordHistograms) return;

                recordFirstContentfulPaint(navigationStartTick / 1000 + firstContentfulPaintMs);
            }
        };
        PageLoadMetrics.addObserver(mPageLoadMetricsObserver);
    }

    /**
     * Marks that startup metrics should be tracked with the |histogramSuffix|.
     * Must only be called on the UI thread.
     */
    public void trackStartupMetrics(String histogramSuffix) {
        mHistogramSuffix = histogramSuffix;
        mShouldTrackStartupMetrics = true;
    }

    public void destroy() {
        mShouldTrackStartupMetrics = false;
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
     * Registers the fact that a navigation has finished. Based on this fact, may discard recording
     * histograms later.
     */
    private void registerFinishNavigation(boolean isTrackedPage) {
        if (!mShouldTrackStartupMetrics) return;

        if (isTrackedPage && UmaUtils.hasComeToForeground() && !UmaUtils.hasComeToBackground()) {
            mFirstCommitTimeMs = SystemClock.uptimeMillis() - mActivityStartTimeMs;
            RecordHistogram.recordMediumTimesHistogram(
                    "Startup.Android.Cold.TimeToFirstNavigationCommit" + mHistogramSuffix,
                    mFirstCommitTimeMs);
        }
        mShouldTrackStartupMetrics = false;
    }

    /**
     * Record the First Contentful Paint time.
     *
     * @param firstContentfulPaintMs timestamp in uptime millis.
     */
    private void recordFirstContentfulPaint(long firstContentfulPaintMs) {
        // First commit time histogram should be recorded before this one. We should discard a
        // record if the first commit time wasn't recorded.
        if (mFirstCommitTimeMs == 0) return;

        if (UmaUtils.hasComeToForeground() && !UmaUtils.hasComeToBackground()) {
            RecordHistogram.recordMediumTimesHistogram(
                    "Startup.Android.Cold.TimeToFirstContentfulPaint" + mHistogramSuffix,
                    firstContentfulPaintMs - mActivityStartTimeMs);
        }
        // This is the last event we track, so destroy this tracker and remove observers.
        destroy();
    }
}
