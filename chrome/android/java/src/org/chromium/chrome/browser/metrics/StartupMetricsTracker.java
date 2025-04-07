// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import android.os.SystemClock;
import android.view.View;

import androidx.annotation.NonNull;

import org.chromium.base.BinderCallsListener;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.base.ColdStartTracker;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.page_load_metrics.PageLoadMetrics;
import org.chromium.chrome.browser.paint_preview.StartupPaintPreviewHelper;
import org.chromium.chrome.browser.paint_preview.StartupPaintPreviewMetrics.PaintPreviewMetricsObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.components.browser_ui.util.FirstDrawDetector;
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

    private static final long TIME_TO_DRAW_METRIC_RECORDING_DELAY_MS = 2500;
    private static final String NTP_COLD_START_HISTOGRAM =
            "Startup.Android.Cold.NewTabPage.TimeToFirstDraw";
    private boolean mFirstNavigationCommitted;

    private class TabObserver extends TabModelSelectorTabObserver {

        private boolean mFirstLoadStarted;

        public TabObserver(TabModelSelector selector) {
            super(selector);
        }

        @Override
        public void onShown(Tab tab, @TabSelectionType int type) {
            if (tab == null) return;

            if (tab.isNativePage()) destroy();
            if (!UrlUtilities.isNtpUrl(tab.getUrl())) mShouldTrackTimeToFirstDraw = false;
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
    private boolean mShouldTrackTimeToFirstDraw = true;
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

    /**
     * Sets up an onDraw listener for the NTP root view to record the NTP cold start metric exactly
     * once per application lifecycle. onDraw will not be called if the screen is off.
     *
     * @param ntpRootView Root view containing the search provider logo (if available), search box,
     *     MV tiles etc.
     */
    public void registerNtpViewObserver(@NonNull View ntpRootView) {
        if (!mShouldTrackTimeToFirstDraw) return;
        trackTimeToFirstDraw(ntpRootView, NTP_COLD_START_HISTOGRAM);
    }

    /**
     * Sets up an onDraw listener for the SearchActivity root view to record the SA cold start
     * metric exactly once per application lifecycle. onDraw will not be called if the screen is
     * off.
     *
     * @param searchActivityRootView SearchActivity's root view.
     */
    public void registerSearchActivityViewObserver(@NonNull View searchActivityRootView) {
        if (!mShouldTrackTimeToFirstDraw) return;
        trackTimeToFirstDraw(
                searchActivityRootView, "Startup.Android.Cold.SearchActivity.TimeToFirstDraw");
    }

    private void trackTimeToFirstDraw(View view, String histogram) {
        if (!SimpleStartupForegroundSessionDetector.runningCleanForegroundSession()
                || !ColdStartTracker.wasColdOnFirstActivityCreationOrNow()) return;

        FirstDrawDetector.waitForFirstDrawStrict(
                view,
                () -> {
                    long timeToFirstDrawMs = SystemClock.uptimeMillis() - mActivityStartTimeMs;
                    if (NTP_COLD_START_HISTOGRAM.equals(histogram)) {
                        recordTimeSpentInBinderCold("NewTabPage");
                    }
                    // During a cold start, first draw can be triggered while Chrome is in
                    // the background, leading to ablated draw times. This early in the startup
                    // process, events that indicate Chrome has been backgrounded do not run until
                    // after the first draw pass. To work around this, post a task to be run with
                    // a delay to record the metric once we can possibly verify if Chrome was ever
                    // sent to the background during startup.
                    PostTask.postDelayedTask(
                            TaskTraits.BEST_EFFORT_MAY_BLOCK,
                            () -> recordTimeToFirstDraw(histogram, timeToFirstDrawMs),
                            TIME_TO_DRAW_METRIC_RECORDING_DELAY_MS);
                    mShouldTrackTimeToFirstDraw = false;
                });
    }

    public void destroy() {
        mShouldTrack = false;
        mShouldTrackTimeToFirstDraw = false;
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
        RecordHistogram.deprecatedRecordMediumTimesHistogram(
                "Startup.Android.Experimental." + name + ".Tabbed.ColdStartTracker", ms);
    }

    private void recordTimeSpentInBinderCold(String variant) {
        Long binderTimeMs = BinderCallsListener.getInstance().getTimeSpentInBinderCalls();
        if (binderTimeMs != null) {
            RecordHistogram.recordMediumTimesHistogram(
                    "Startup.Android.Cold." + variant + ".TimeSpentInBinder", binderTimeMs);
        }
    }

    private void recordNavigationCommitMetrics(long firstCommitMs) {
        if (!SimpleStartupForegroundSessionDetector.runningCleanForegroundSession()) return;
        if (ColdStartTracker.wasColdOnFirstActivityCreationOrNow()) {
            RecordHistogram.deprecatedRecordMediumTimesHistogram(
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
            RecordHistogram.deprecatedRecordMediumTimesHistogram(
                    "Startup.Android.Cold.TimeToFirstContentfulPaint3.Tabbed", firstFcpMs);
        }
    }

    private void recordTimeToFirstVisibleContent(long durationMs) {
        if (mFirstVisibleContentRecorded) return;

        mFirstVisibleContentRecorded = true;
        RecordHistogram.deprecatedRecordMediumTimesHistogram(
                "Startup.Android.Cold.TimeToFirstVisibleContent4", durationMs);
    }

    private void recordFirstSafeBrowsingResponseTime() {
        if (mFirstSafeBrowsingResponseTimeRecorded) return;
        mFirstSafeBrowsingResponseTimeRecorded = true;

        if (mFirstSafeBrowsingResponseTimeMicros != 0) {
            RecordHistogram.deprecatedRecordMediumTimesHistogram(
                    "Startup.Android.Cold.FirstSafeBrowsingApiResponseTime2.Tabbed",
                    mFirstSafeBrowsingResponseTimeMicros / 1000);
        }
    }

    /**
     * Records a histogram capturing the time taken from a cold start to the first draw event.
     *
     * <p>This function logs the time elapsed from application launch to the first draw event,
     * categorized by what was drawn first (e.g., NTP or SearchActivity).
     *
     * @param histogramName The name of the histogram, indicating what was drawn first.
     * @param timeToFirstDrawMs The elapsed time in milliseconds from cold start to the first draw
     *     event.
     */
    private void recordTimeToFirstDraw(String histogramName, long timeToFirstDrawMs) {
        if (!SimpleStartupForegroundSessionDetector.runningCleanForegroundSession()
                || !ColdStartTracker.wasColdOnFirstActivityCreationOrNow()) return;
        RecordHistogram.recordMediumTimesHistogram(histogramName, timeToFirstDrawMs);
    }
}
