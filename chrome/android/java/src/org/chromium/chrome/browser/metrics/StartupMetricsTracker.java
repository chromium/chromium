// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import android.app.ActivityManager;
import android.app.ApplicationStartInfo;
import android.content.Context;
import android.os.Build;
import android.os.Process;
import android.os.SystemClock;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.RequiresApi;

import org.chromium.base.BinderCallsListener;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TimeUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
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

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;
import java.util.function.Supplier;

/**
 * Records UMA page load metrics for the first navigation on a cold start.
 *
 * <p>Uses different cold start heuristics from {@link LegacyTabStartupMetricsTracker}. These
 * heuristics aim to replace a few metrics from Startup.Android.Cold.*.
 */
@NullMarked
public class StartupMetricsTracker {
    private static final long TIME_TO_DRAW_METRIC_RECORDING_DELAY_MS = 2500;
    private static final String NTP_COLD_START_HISTOGRAM =
            "Startup.Android.Cold.NewTabPage.TimeToFirstDraw";
    private static final String TIME_TO_STARTUP_FCP_OR_PAINT_PREVIEW_HISTOGRAM =
            "Startup.Android.Cold.TimeToStartupFcpOrPaintPreview";
    private static final String COLD_START_TIME_TO_FIRST_FRAME =
            "Startup.Android.Cold.TimeToFirstFrame";
    private static final String COLD_START_MISMATCH_HISTOGRAM =
            "Startup.Android.Cold.TemperatureMismatch";
    private static final String COLD_START_EXPERIMENTAL_FCP_TABBED_HISTOGRAM =
            "Startup.Android.Cold.ExperimentalProcessStart.TimeToFirstContentfulPaint.Tabbed";
    private static final String COLD_START_EXPERIMENTAL_FIRST_VISIBLE_CONTENT_HISTOGRAM =
            "Startup.Android.Cold.ExperimentalProcessStart.TimeToFirstVisibleContent";
    private boolean mFirstNavigationCommitted;

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    //
    // LINT.IfChange(AndroidStartupTemperature)
    @IntDef({
        AndroidStartupTemperature.UNSET,
        AndroidStartupTemperature.COLD,
        AndroidStartupTemperature.WARM,
        AndroidStartupTemperature.HOT,
        AndroidStartupTemperature.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface AndroidStartupTemperature {
        int UNSET = 0;
        int COLD = 1;
        int WARM = 2;
        int HOT = 3;
        int NUM_ENTRIES = 4;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/startup/enums.xml:AndroidStartupTemperature)

    // LINT.IfChange(AndroidColdStartMismatchLocation)
    @IntDef({
        AndroidColdStartMismatchLocation.TRACKER_COLD_SYSTEM_NOT_COLD_ACTIVITY,
        AndroidColdStartMismatchLocation.TRACKER_COLD_SYSTEM_NOT_COLD_OTHER,
        AndroidColdStartMismatchLocation.TRACKER_NOT_COLD_SYSTEM_COLD_ACTIVITY,
        AndroidColdStartMismatchLocation.TRACKER_NOT_COLD_SYSTEM_COLD_OTHER,
        AndroidColdStartMismatchLocation.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface AndroidColdStartMismatchLocation {
        int TRACKER_COLD_SYSTEM_NOT_COLD_ACTIVITY = 0;
        int TRACKER_COLD_SYSTEM_NOT_COLD_OTHER = 1;
        int TRACKER_NOT_COLD_SYSTEM_COLD_ACTIVITY = 2;
        int TRACKER_NOT_COLD_SYSTEM_COLD_OTHER = 3;
        int NUM_ENTRIES = 4;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/startup/enums.xml:AndroidColdStartMismatchLocation)

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
        public void onDidFinishNavigationInPrimaryMainFrame(Tab tab, NavigationHandle navigation) {
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
                recordNavigationCommitMetrics();
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
    // The {@link SystemClock#uptimeMillis()} at which this process was started, but before any of
    // the application was executed.
    private final long mProcessStartTimeMs;
    private Supplier<Boolean> mIsRestoringPersistentStateSupplier;
    private boolean mFirstVisibleContentRecorded;
    private boolean mTimeToStartupFcpOrPaintPreviewRecorded;
    private @Nullable TabModelSelectorTabObserver mTabObserver;
    private @Nullable PageObserver mPageObserver;
    private boolean mShouldTrack = true;
    private boolean mShouldTrackTimeToFirstDraw = true;
    private boolean mActivityStartInfoMetricsRecorded;
    private @ActivityType int mHistogramSuffix;
    // The time it took for SafeBrowsing API to return a Safe Browsing response for the first time.
    // The SB request is on the critical path to navigation commit, and the response may be severely
    // delayed by GmsCore (see http://crbug.com/1296097). The value is recorded only when the
    // navigation commits successfully and the URL of first navigation is checked by SafeBrowsing
    // API. Utilizing a volatile long here to ensure the write is immediately visible to other
    // threads.
    private volatile long mFirstSafeBrowsingResponseTimeMicros;
    private boolean mFirstSafeBrowsingResponseTimeRecorded;

    public StartupMetricsTracker(
            MonotonicObservableSupplier<TabModelSelector> tabModelSelectorSupplier,
            Supplier<Boolean> isRestoringPersistentStateSupplier) {
        mActivityStartTimeMs = SystemClock.uptimeMillis();
        mProcessStartTimeMs = Process.getStartUptimeMillis();
        mIsRestoringPersistentStateSupplier = isRestoringPersistentStateSupplier;
        tabModelSelectorSupplier.addSyncObserverAndPostIfNonNull(this::registerObservers);
        SafeBrowsingApiBridge.setOneTimeSafeBrowsingApiUrlCheckObserver(
                this::updateSafeBrowsingCheckTime);
    }

    /**
     * Sets up a listener for ApplicationStartInfo that will eventually report TimeToFirstFrame once
     * per application lifecycle.
     */
    @RequiresApi(35)
    public void registerApplicationStartInfoListener() {
        ActivityManager activityManager =
                (ActivityManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.ACTIVITY_SERVICE);
        activityManager.addApplicationStartInfoCompletionListener(
                PostTask.getUiBestEffortExecutor(), this::recordTimeToFirstFrame);
    }

    /**
     * Returns the most recent ApplicationStartInfo at the time the request is made. May or may not
     * contain certain bits of information at the time of the request - see the API for more
     * details. Makes a Binder transaction so call on a background thread.
     *
     * @return Returns an ApplicationStartInfo if available for the current start or null.
     */
    @RequiresApi(35)
    public static @Nullable ApplicationStartInfo getCurrentApplicationStartInfo() {
        ThreadUtils.assertOnBackgroundThread();
        ActivityManager activityManager =
                (ActivityManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.ACTIVITY_SERVICE);
        List<ApplicationStartInfo> startInfos = activityManager.getHistoricalProcessStartReasons(1);
        if (startInfos == null || startInfos.isEmpty()) return null;
        return startInfos.get(0);
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
                        recordTimeToStartupFcpOrPaintPreview(durationMs);
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
    public void registerNtpViewObserver(View ntpRootView) {
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
    public void registerSearchActivityViewObserver(View searchActivityRootView) {
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
                        recordBinderMetricsCold("NewTabPage");
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

    @SuppressWarnings("NullAway")
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
        if (mIsRestoringPersistentStateSupplier != null) {
            mIsRestoringPersistentStateSupplier = null;
        }
    }

    private String activityTypeToSuffix(@ActivityType int type) {
        if (type == ActivityType.TABBED) return ".Tabbed";
        assert type == ActivityType.WEB_APK;
        return ".WebApk";
    }

    private void recordBinderMetricsCold(String variant) {
        BinderCallsListener binderListener = BinderCallsListener.getInstance();
        if (!binderListener.isInstalled()) {
            return;
        }
        long binderTimeMs = binderListener.getTimeSpentInBinderCalls();
        RecordHistogram.recordMediumTimesHistogram(
                "Startup.Android.Cold." + variant + ".TimeSpentInBinder", binderTimeMs);
        int binderCallCount = binderListener.getTotalBinderTransactionsCount();
        RecordHistogram.recordCount1000Histogram(
                "Startup.Android.Cold." + variant + ".TotalBinderTransactions", binderCallCount);
    }

    private void recordNavigationCommitMetrics() {
        long currentTimeMs = SystemClock.uptimeMillis();
        if (!SimpleStartupForegroundSessionDetector.runningCleanForegroundSession()) return;
        if (ColdStartTracker.wasColdOnFirstActivityCreationOrNow()) {
            long activityDurationMs = currentTimeMs - mActivityStartTimeMs;
            RecordHistogram.deprecatedRecordMediumTimesHistogram(
                    "Startup.Android.Cold.TimeToFirstNavigationCommit3"
                            + activityTypeToSuffix(mHistogramSuffix),
                    activityDurationMs);
            long processDurationMs = currentTimeMs - mProcessStartTimeMs;
            RecordHistogram.recordMediumTimesHistogram(
                    "Startup.Android.Cold.ExperimentalProcessStart.TimeToFirstNavigationCommit"
                            + activityTypeToSuffix(mHistogramSuffix),
                    processDurationMs);
            if (mHistogramSuffix == ActivityType.TABBED) {
                recordFirstSafeBrowsingResponseTime();
                recordTimeToFirstVisibleContent(activityDurationMs);
            }
        }
    }

    private void recordFcpMetrics(long firstFcpMs) {
        if (!SimpleStartupForegroundSessionDetector.runningCleanForegroundSession()) return;
        if (ColdStartTracker.wasColdOnFirstActivityCreationOrNow()) {
            if (mIsRestoringPersistentStateSupplier != null
                    && mIsRestoringPersistentStateSupplier.get()) {
                RecordHistogram.deprecatedRecordMediumTimesHistogram(
                        "Startup.Android.Cold.WithPersistentState."
                                + "TimeToFirstContentfulPaint3.Tabbed",
                        firstFcpMs);
            } else {
                RecordHistogram.deprecatedRecordMediumTimesHistogram(
                        "Startup.Android.Cold.TimeToFirstContentfulPaint3.Tabbed", firstFcpMs);
                RecordHistogram.recordMediumTimesHistogram(
                        COLD_START_EXPERIMENTAL_FCP_TABBED_HISTOGRAM,
                        firstFcpMs + (mActivityStartTimeMs - mProcessStartTimeMs));
            }
            recordTimeToStartupFcpOrPaintPreview(firstFcpMs);
        }
    }

    private void recordTimeToFirstVisibleContent(long durationMs) {
        if (mFirstVisibleContentRecorded) return;

        mFirstVisibleContentRecorded = true;
        if (mIsRestoringPersistentStateSupplier != null
                && mIsRestoringPersistentStateSupplier.get()) {
            RecordHistogram.deprecatedRecordMediumTimesHistogram(
                    "Startup.Android.Cold.WithPersistentState.TimeToFirstVisibleContent4",
                    durationMs);
        } else {
            RecordHistogram.deprecatedRecordMediumTimesHistogram(
                    "Startup.Android.Cold.TimeToFirstVisibleContent4", durationMs);
            RecordHistogram.recordMediumTimesHistogram(
                    COLD_START_EXPERIMENTAL_FIRST_VISIBLE_CONTENT_HISTOGRAM,
                    durationMs + (mActivityStartTimeMs - mProcessStartTimeMs));
        }
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

    /**
     * Records a histogram capturing TimeToStartupFcpOrPaintPreview.
     *
     * <p>This metric reports the minimum value of
     * Startup.Android.Cold.TimeToFirstContentfulPaint3.Tabbed and
     * Browser.PaintPreview.TabbedPlayer.TimeToFirstBitmap.
     *
     * @param durationMs duration in millis.
     */
    private void recordTimeToStartupFcpOrPaintPreview(long durationMs) {
        if (mTimeToStartupFcpOrPaintPreviewRecorded) return;
        mTimeToStartupFcpOrPaintPreviewRecorded = true;
        RecordHistogram.recordMediumTimesHistogram(
                TIME_TO_STARTUP_FCP_OR_PAINT_PREVIEW_HISTOGRAM, durationMs);
    }

    /**
     * Records a histogram capturing TimeToFirstFrame.
     *
     * <p>This metric reports the the time it takes from activity start until Android determines the
     * first frame of the app has been drawn.
     *
     * @param applicationStartInfo contains various bits of information regarding app startup.
     */
    @RequiresApi(35)
    private void recordTimeToFirstFrame(ApplicationStartInfo applicationStartInfo) {
        if (!SimpleStartupForegroundSessionDetector.runningCleanForegroundSession()
                || mActivityStartInfoMetricsRecorded) return;

        boolean isTrackerCold = ColdStartTracker.wasColdOnFirstActivityCreationOrNow();
        boolean isSystemCold =
                applicationStartInfo.getStartType() == ApplicationStartInfo.START_TYPE_COLD;
        if (isTrackerCold != isSystemCold && Build.VERSION.SDK_INT >= Build.VERSION_CODES.BAKLAVA) {
            recordMismatchHistogram(applicationStartInfo, isTrackerCold);
        }

        // TODO(crbug.com/463329742): Replace ColdStartTracker with ApplicationStartInfo when
        // test-related cold-start tracking issues are mitigated.
        if (!isTrackerCold) return;
        mActivityStartInfoMetricsRecorded = true;
        final long firstFrameTimeMs =
                applicationStartInfo
                                .getStartupTimestamps()
                                .getOrDefault(ApplicationStartInfo.START_TIMESTAMP_FIRST_FRAME, 0L)
                        / TimeUtils.NANOSECONDS_PER_MILLISECOND;
        if (firstFrameTimeMs != 0L && mActivityStartTimeMs < firstFrameTimeMs) {
            RecordHistogram.recordMediumTimesHistogram(
                    COLD_START_TIME_TO_FIRST_FRAME, firstFrameTimeMs - mActivityStartTimeMs);
        }
    }

    /**
     * Records a histogram capturing TemperatureMismatch context.
     *
     * <p>This metric records context around how a cold start detection mismatch occurred using
     * ColdStartTracker (Clank's solution) and ApplicationStartInfo (Android API). If there is a
     * mismatch, see if Clank and Android agree on what component caused this launch.
     *
     * @param applicationStartInfo contains various bits of information regarding app startup.
     * @param isTrackerCold boolean that determines whether Clank had a cold start based on whether
     *     the start was caused by an Activity launch.
     */
    @RequiresApi(36)
    private void recordMismatchHistogram(
            ApplicationStartInfo applicationStartInfo, boolean isTrackerCold) {
        boolean isSystemActivity =
                applicationStartInfo.getStartComponent()
                        == ApplicationStartInfo.START_COMPONENT_ACTIVITY;

        @AndroidColdStartMismatchLocation int sample;
        if (isTrackerCold) {
            sample =
                    isSystemActivity
                            ? AndroidColdStartMismatchLocation.TRACKER_COLD_SYSTEM_NOT_COLD_ACTIVITY
                            : AndroidColdStartMismatchLocation.TRACKER_COLD_SYSTEM_NOT_COLD_OTHER;
        } else {
            sample =
                    isSystemActivity
                            ? AndroidColdStartMismatchLocation.TRACKER_NOT_COLD_SYSTEM_COLD_ACTIVITY
                            : AndroidColdStartMismatchLocation.TRACKER_NOT_COLD_SYSTEM_COLD_OTHER;
        }

        RecordHistogram.recordEnumeratedHistogram(
                COLD_START_MISMATCH_HISTOGRAM,
                sample,
                AndroidColdStartMismatchLocation.NUM_ENTRIES);
    }
}
