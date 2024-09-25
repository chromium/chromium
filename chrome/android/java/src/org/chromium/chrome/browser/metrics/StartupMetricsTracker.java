// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import android.os.SystemClock;

import androidx.annotation.NonNull;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.base.ColdStartTracker;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.page_load_metrics.PageLoadMetrics;
import org.chromium.chrome.browser.paint_preview.StartupPaintPreviewHelper;
import org.chromium.chrome.browser.paint_preview.StartupPaintPreviewMetrics.PaintPreviewMetricsObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.safe_browsing.SafeBrowsingApiBridge;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

/**
 * Records UMA page load metrics for the first navigation on a cold start.
 *
 * <p>Uses different cold start heuristics from {@link LegacyTabStartupMetricsTracker}. These
 * heuristics aim to replace a few metrics from Startup.Android.Cold.*.
 */
public class StartupMetricsTracker {

    private boolean mFirstNavigationCommitted;

    private class TabObserver extends TabModelSelectorTabObserver {

        private boolean mFirstLoadStarted;

        public TabObserver(TabModelSelector selector) {
            super(selector);
        }

        @Override
        public void onShown(Tab tab, @TabSelectionType int type) {
            if (tab != null && tab.isNativePage()) {
                // Avoid recording metrics when the NTP is shown.
                destroy();
            }
        }

        @Override
        public void onPageLoadStarted(Tab tab, GURL url) {
            // Discard startup navigation measurements when the user started another navigation.
            if (!mFirstLoadStarted) {
                mFirstLoadStarted = true;
            } else {
                destroy();
            }
        }

        @Override
        public void onDidFinishNavigationInPrimaryMainFrame(
                Tab tab, @NonNull NavigationHandle navigation) {
            if (!mShouldTrack || mFirstNavigationCommitted) return;
            boolean shouldTrack =
                    navigation.hasCommitted()
                            && !navigation.isErrorPage()
                            && UrlUtilities.isHttpOrHttps(navigation.getUrl())
                            && !navigation.isSameDocument();
            if (!shouldTrack) {
                // When navigation leads to an error page, download or chrome:// URLs, avoid
                // recording both commit and FCP.
                //
                // In rare cases a same-document navigation can commit before all other
                // http(s)+non-error navigations (crbug.com/1492721). Filter out such scenarios
                // since they are counter-intuitive.
                destroy();
            } else {
                mFirstNavigationCommitted = true;
                recordNavigationCommitMetrics(SystemClock.uptimeMillis() - mActivityStartTimeMs);
            }
        }
    }

    private class PageObserver implements PageLoadMetrics.Observer {
        private static final long NO_NAVIGATION_ID = -1;
        private long mNavigationId = NO_NAVIGATION_ID;

        @Override
        public void onNewNavigation(
                WebContents webContents,
                long navigationId,
                boolean isFirstNavigationInWebContents) {
            if (mNavigationId != NO_NAVIGATION_ID) return;
            mNavigationId = navigationId;
        }

        @Override
        public void onFirstContentfulPaint(
                WebContents webContents,
                long navigationId,
                long navigationStartMicros,
                long firstContentfulPaintMs) {
            recordFcpMetricsIfNeeded(navigationId, navigationStartMicros, firstContentfulPaintMs);
            destroy();
        }

        private void recordFcpMetricsIfNeeded(
                long navigationId, long navigationStartMicros, long firstContentfulPaintMs) {
            if (navigationId != mNavigationId || !mShouldTrack || !mFirstNavigationCommitted) {
                return;
            }
            recordFcpMetrics(
                    navigationStartMicros / 1000 + firstContentfulPaintMs - mActivityStartTimeMs);
        }
    }

    // The time of the activity onCreate(). All metrics (such as time to first visible content) are
    // reported in uptimeMillis relative to this value.
    private final long mActivityStartTimeMs;
    private boolean mFirstVisibleContentRecorded;

    private TabModelSelectorTabObserver mTabObserver;
    private PageObserver mPageObserver;
    private boolean mShouldTrack = true;
    private @ActivityType int mHistogramSuffix;

    // The time it took for SafeBrowsing API to return a Safe Browsing response for the first time.
    // The SB request is on the critical path to navigation commit, and the response may be severely
    // delayed by GmsCore (see http://crbug.com/1296097). The value is recorded only when the
    // navigation commits successfully and the URL of first navigation is checked by SafeBrowsing
    // API. Utilizing a volatile long here to ensure the write is immediately visible to other
    // threads.
    private volatile long mFirstSafeBrowsingResponseTimeMicros;
    private boolean mFirstSafeBrowsingResponseTimeRecorded;

    public StartupMetricsTracker(ObservableSupplier<TabModelSelector> tabModelSelectorSupplier) {
        mActivityStartTimeMs = SystemClock.uptimeMillis();
        tabModelSelectorSupplier.addObserver(this::registerObservers);
        SafeBrowsingApiBridge.setOneTimeSafeBrowsingApiUrlCheckObserver(
                this::updateSafeBrowsingCheckTime);
    }

    private void updateSafeBrowsingCheckTime(long urlCheckTimeDeltaMicros) {
        mFirstSafeBrowsingResponseTimeMicros = urlCheckTimeDeltaMicros;
    }

    /**
     * Choose the UMA histogram to record later. The {@link ActivityType} parameter indicates the
     * kind of startup scenario to track.
     *
     * @param activityType Either TABBED or WEB_APK.
     */
    public void setHistogramSuffix(@ActivityType int activityType) {
        mHistogramSuffix = activityType;
    }

    private void registerObservers(TabModelSelector tabModelSelector) {
        if (!mShouldTrack) return;
        mTabObserver = new TabObserver(tabModelSelector);
        mPageObserver = new PageObserver();
        PageLoadMetrics.addObserver(mPageObserver, /* supportPrerendering= */ false);
    }

    /**
     * Register an observer to be notified on the first paint of a paint preview if present.
     *
     * @param startupPaintPreviewHelper the helper to register the observer to.
     */
    public void registerPaintPreviewObserver(StartupPaintPreviewHelper startupPaintPreviewHelper) {
        startupPaintPreviewHelper.addMetricsObserver(
                new PaintPreviewMetricsObserver() {
                    @Override
                    public void onFirstPaint(long durationMs) {
                        recordTimeToFirstVisibleContent(durationMs);
                    }

                    @Override
                    public void onUnrecordedFirstPaint() {}
                });
    }

    public void destroy() {
        mShouldTrack = false;
        if (mTabObserver != null) {
            mTabObserver.destroy();
            mTabObserver = null;
        }
        if (mPageObserver != null) {
            PageLoadMetrics.removeObserver(mPageObserver);
            mPageObserver = null;
        }
    }

    private String activityTypeToSuffix(@ActivityType int type) {
        if (type == ActivityType.TABBED) return ".Tabbed";
        assert type == ActivityType.WEB_APK;
        return ".WebApk";
    }

    private void recordExperimentalHistogram(String name, long ms) {
        RecordHistogram.recordMediumTimesHistogram(
                "Startup.Android.Experimental." + name + ".Tabbed.ColdStartTracker", ms);
    }

    private void recordNavigationCommitMetrics(long firstCommitMs) {
        if (!SimpleStartupForegroundSessionDetector.runningCleanForegroundSession()) return;
        if (ColdStartTracker.wasColdOnFirstActivityCreationOrNow()) {
            RecordHistogram.recordMediumTimesHistogram(
                    "Startup.Android.Cold.TimeToFirstNavigationCommit3"
                            + activityTypeToSuffix(mHistogramSuffix),
                    firstCommitMs);
            if (mHistogramSuffix == ActivityType.TABBED) {
                recordExperimentalHistogram("FirstNavigationCommit", firstCommitMs);
                recordFirstSafeBrowsingResponseTime();
                recordTimeToFirstVisibleContent(firstCommitMs);
            }
        }
    }

    private void recordFcpMetrics(long firstFcpMs) {
        if (!SimpleStartupForegroundSessionDetector.runningCleanForegroundSession()) return;
        if (ColdStartTracker.wasColdOnFirstActivityCreationOrNow()) {
            recordExperimentalHistogram("FirstContentfulPaint", firstFcpMs);
            RecordHistogram.recordMediumTimesHistogram(
                    "Startup.Android.Cold.TimeToFirstContentfulPaint3.Tabbed", firstFcpMs);
        }
    }

    private void recordTimeToFirstVisibleContent(long durationMs) {
        if (mFirstVisibleContentRecorded) return;

        mFirstVisibleContentRecorded = true;
        RecordHistogram.recordMediumTimesHistogram(
                "Startup.Android.Cold.TimeToFirstVisibleContent4", durationMs);
    }

    private void recordFirstSafeBrowsingResponseTime() {
        if (mFirstSafeBrowsingResponseTimeRecorded) return;
        mFirstSafeBrowsingResponseTimeRecorded = true;

        if (mFirstSafeBrowsingResponseTimeMicros != 0) {
            RecordHistogram.recordMediumTimesHistogram(
                    "Startup.Android.Cold.FirstSafeBrowsingApiResponseTime2.Tabbed",
                    mFirstSafeBrowsingResponseTimeMicros / 1000);
        }
    }
}
