// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.chromium.build.NullUtil.assertNonNull;

import android.content.Intent;
import android.net.Uri;
import android.os.Process;
import android.os.SystemClock;
import android.text.TextUtils;
import android.text.format.DateUtils;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.base.ColdStartTracker;
import org.chromium.chrome.browser.browserservices.intents.SessionHolder;
import org.chromium.chrome.browser.customtabs.ClientManager.CalledWarmup;
import org.chromium.chrome.browser.customtabs.features.TabInteractionRecorder;
import org.chromium.chrome.browser.intents.BrowserIntentUtils;
import org.chromium.chrome.browser.metrics.SimpleStartupForegroundSessionDetector;
import org.chromium.chrome.browser.page_load_metrics.PageLoadMetrics;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.Tab.LoadUrlResult;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.components.ukm.UkmRecorder;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/** A {@link TabObserver} that also handles custom tabs specific logging and messaging. */
@NullMarked
public class CustomTabObserver extends EmptyTabObserver {
    private final @Nullable CustomTabsConnection mCustomTabsConnection;
    private final @Nullable SessionHolder<?> mSession;

    private final NavigationInfoCaptureTrigger mNavigationInfoCaptureTrigger =
            new NavigationInfoCaptureTrigger(this::captureNavigationInfo);

    // The time at which Chrome received the intent that resulted in the most recent Custom Tab
    // launch, in Realtime and Uptime timebases - not set when the mayLaunchUrl speculation is used.
    private long mIntentReceivedRealtimeMillis;
    private long mIntentReceivedUptimeMillis;

    // The time at which Chrome received the intent that resulted in the most recent Custom Tab
    // launch, in Realtime and Uptime timebases, when the mayLaunchUrl API was used.
    private long mLaunchedForSpeculationRealtimeMillis;
    private long mLaunchedForSpeculationUptimeMillis;

    // true/false if the mayLaunchUrl API was used and the speculation was used/not used. null if
    // the API was not used.
    private @Nullable Boolean mUsedHiddenTabSpeculation;

    // The time of the first navigation commit in the most recent Custom Tab launch.
    private long mFirstCommitRealtimeMillis;
    private long mFirstCommitUptimeMillis;

    // Version of android-browser-helper, or zero if not reported
    private int mBrowserHelperVersion;
    // System.uptimeMillis() when the TWA wrapper activity has started, or zero if not reported
    private long mTwaStartupUptimeMillis;
    private final List<Runnable> mTwaStartupTimeAvailableCallbacks = new ArrayList<>();

    // Lets Long press on links select the link text instead of triggering context menu.
    private boolean mLongPressLinkSelectText;

    @IntDef({State.RESET, State.WAITING_LOAD_START, State.WAITING_LOAD_FINISH})
    @Retention(RetentionPolicy.SOURCE)
    @interface State {
        int RESET = 0;
        int WAITING_LOAD_START = 1;
        int WAITING_LOAD_FINISH = 2;
    }

    // Tracks what point in the first navigation after a Custom Tab launch we're in.
    private @State int mCurrentState;

    private @Nullable PageLoadMetricsObserver mPageLoadMetricsObserver;

    private class PageLoadMetricsObserver implements PageLoadMetrics.Observer {
        @Override
        public void onFirstContentfulPaint(
                WebContents webContents,
                long navigationId,
                long navigationStartMicros,
                long firstContentfulPaintMs) {
            recordFirstContentfulPaint(
                    webContents, navigationStartMicros / 1000 + firstContentfulPaintMs);
        }

        @Override
        public void onLargestContentfulPaint(
                WebContents webContents,
                long navigationId,
                long navigationStartMicros,
                long largestContentfulPaintMs,
                long largestContentfulPaintSize) {
            recordLargestContentfulPaint(
                    webContents, navigationStartMicros / 1000 + largestContentfulPaintMs);
            PageLoadMetrics.removeObserver(assertNonNull(mPageLoadMetricsObserver));
            mPageLoadMetricsObserver = null;
        }

        @Override
        public void onUserTimingMarkFullyLoaded(
                WebContents webContents,
                long navigationId,
                long navigationStartMicros,
                long markFullyLoadedMs) {
            recordUserTiming(
                    webContents,
                    navigationStartMicros / 1000 + markFullyLoadedMs,
                    "TimeToMarkFullyLoaded");
        }

        @Override
        public void onUserTimingMarkFullyVisible(
                WebContents webContents,
                long navigationId,
                long navigationStartMicros,
                long markFullyVisibleMs) {
            recordUserTiming(
                    webContents,
                    navigationStartMicros / 1000 + markFullyVisibleMs,
                    "TimeToMarkFullyVisible");
        }

        @Override
        public void onUserTimingMarkInteractive(
                WebContents webContents,
                long navigationId,
                long navigationStartMicros,
                long markInteractiveMs) {
            recordUserTiming(
                    webContents,
                    navigationStartMicros / 1000 + markInteractiveMs,
                    "TimeToMarkInteractive");
        }
    }

    public CustomTabObserver(boolean openedByChrome, @Nullable SessionHolder<?> token) {
        mCustomTabsConnection = openedByChrome ? null : CustomTabsConnection.getInstance();
        mSession = token;
        resetPageLoadTracking();
    }

    private void trackNextLCP() {
        if (mPageLoadMetricsObserver != null) return;
        mPageLoadMetricsObserver = new PageLoadMetricsObserver();
        PageLoadMetrics.addObserver(mPageLoadMetricsObserver, true);
    }

    /**
     * Tracks the next page load, with timestamp as the origin of time. If a load is already
     * happening, we track its PLT. If not, we track NavigationCommit timing + PLT for the next
     * load.
     */
    public void trackNextPageLoadForLaunch(Tab tab, Intent sourceIntent) {
        mIntentReceivedRealtimeMillis = BrowserIntentUtils.getLaunchedRealtimeMillis(sourceIntent);
        mIntentReceivedUptimeMillis = BrowserIntentUtils.getLaunchedUptimeMillis(sourceIntent);
        if (tab.isLoading()) {
            mCurrentState = State.WAITING_LOAD_FINISH;
        } else {
            mCurrentState = State.WAITING_LOAD_START;
        }
        trackNextLCP();
    }

    public void trackNextPageLoadForHiddenTab(
            WebContents webContents,
            boolean usedSpeculation,
            boolean hasCommitted,
            Intent sourceIntent) {
        // If page load is already being tracked, it must have been an early nav - nothing to do
        // here.
        if (mIntentReceivedRealtimeMillis != 0) return;
        mUsedHiddenTabSpeculation = usedSpeculation;
        mLaunchedForSpeculationRealtimeMillis =
                BrowserIntentUtils.getLaunchedRealtimeMillis(sourceIntent);
        mLaunchedForSpeculationUptimeMillis =
                BrowserIntentUtils.getLaunchedUptimeMillis(sourceIntent);
        trackNextLCP();
        if (usedSpeculation && hasCommitted) {
            recordFirstCommitNavigation(webContents);
        }
    }

    /**
     * Enable/disable the behavior of long press on links selecting text instead of triggering
     * context menu.
     *
     * @param tab Tab to apply the behavior to for its WebContents.
     * @param enabled Behavior enabled or not.
     */
    public void setLongPressLinkSelectText(Tab tab, boolean enabled) {
        mLongPressLinkSelectText = enabled;
        setLongPressLinkSelectTextInternal(tab);
    }

    private void setLongPressLinkSelectTextInternal(Tab tab) {
        WebContents webContents = tab.getWebContents();
        if (webContents == null) return;
        webContents.setLongPressLinkSelectText(mLongPressLinkSelectText);
    }

    @Override
    public void onContentChanged(Tab tab) {
        setLongPressLinkSelectTextInternal(tab);
    }

    @Override
    public void onLoadUrl(Tab tab, LoadUrlParams params, LoadUrlResult loadUrlResult) {
        if (mCustomTabsConnection != null) {
            mCustomTabsConnection.registerLaunch(mSession, params.getUrl());
        }
    }

    @Override
    public void onPageLoadStarted(Tab tab, GURL url) {
        if (mCurrentState == State.WAITING_LOAD_START) {
            mCurrentState = State.WAITING_LOAD_FINISH;
        } else if (mCurrentState == State.WAITING_LOAD_FINISH) {
            if (mCustomTabsConnection != null) {
                mCustomTabsConnection.sendNavigationInfo(
                        mSession, tab.getUrl().getSpec(), tab.getTitle(), (Uri) null);
            }
        }
        if (mCustomTabsConnection != null) {
            mCustomTabsConnection.setSendNavigationInfoForSession(mSession, false);
            mNavigationInfoCaptureTrigger.onNewNavigation();
        }
    }

    @Override
    public void onHidden(Tab tab, @TabHidingType int reason) {
        mNavigationInfoCaptureTrigger.onHide(tab);
    }

    @Override
    public void onPageLoadFinished(Tab tab, GURL url) {
        resetPageLoadTracking();
        mNavigationInfoCaptureTrigger.onLoadFinished(tab);
    }

    @Override
    public void onPageLoadFailed(Tab tab, int errorCode) {
        resetPageLoadTracking();
    }

    private boolean wasWarmedUp() {
        if (mCustomTabsConnection == null) return false;
        @CalledWarmup int warmedState = mCustomTabsConnection.getWarmupState(mSession);
        return warmedState == CalledWarmup.SESSION_NO_WARMUP_ALREADY_CALLED
                || warmedState == CalledWarmup.SESSION_WARMUP
                || warmedState == CalledWarmup.NO_SESSION_WARMUP;
    }

    @Override
    public void onDidFinishNavigationInPrimaryMainFrame(Tab tab, NavigationHandle navigation) {
        boolean firstNavigation = mFirstCommitRealtimeMillis == 0;
        boolean isFirstMainFrameCommit =
                firstNavigation
                        && navigation.hasCommitted()
                        && !navigation.isErrorPage()
                        && !navigation.isSameDocument();
        if (!isFirstMainFrameCommit) return;

        mFirstCommitRealtimeMillis = SystemClock.elapsedRealtime();
        mFirstCommitUptimeMillis = SystemClock.uptimeMillis();

        recordFirstCommitNavigation(assertNonNull(tab.getWebContents()));
    }

    private void recordFirstCommitNavigation(WebContents webContents) {
        if (mCustomTabsConnection == null) return;
        String suffix = null;
        long duration = 0;
        // Note that this will exclude Webapp launches in all cases due to either
        // mUsedHiddenTabSpeculation being null, or mIntentReceivedTimestamp being 0.
        if (mUsedHiddenTabSpeculation != null && mUsedHiddenTabSpeculation) {
            duration = mFirstCommitRealtimeMillis - mLaunchedForSpeculationRealtimeMillis;
            suffix = ".Speculated";
        } else if (mIntentReceivedRealtimeMillis > 0) {
            // When the process is already warm the earliest measurable point in startup is when the
            // intent is received so we measure from there. In the cold start case we measure from
            // when the process was started as the best comparison against the warm case.
            if (wasWarmedUp()) {
                duration = mFirstCommitRealtimeMillis - mIntentReceivedRealtimeMillis;
                suffix = ".WarmedUp";
            } else if (ColdStartTracker.wasColdOnFirstActivityCreationOrNow()
                    && SimpleStartupForegroundSessionDetector.runningCleanForegroundSession()) {
                duration = mFirstCommitRealtimeMillis - Process.getStartElapsedRealtime();
                suffix = ".Cold";
            } else {
                duration = mFirstCommitRealtimeMillis - mIntentReceivedRealtimeMillis;
                suffix = ".Warm";
            }
        }
        if (suffix != null) {
            RecordHistogram.recordCustomTimesHistogram(
                    "CustomTabs.Startup.TimeToFirstCommitNavigation2" + suffix,
                    duration,
                    50,
                    DateUtils.MINUTE_IN_MILLIS,
                    50);
        }
        callOnTwaStartupTimeAvailable(
                () -> {
                    long twaDuration = mFirstCommitUptimeMillis - mTwaStartupUptimeMillis;
                    // The TWA durations are always relative to the startup time passed in the
                    // Intent.
                    RecordHistogram.recordCustomTimesHistogram(
                            "TrustedWebActivity.Startup.TimeToFirstCommitNavigation2"
                                    + (isTwaColdStart() ? ".Cold" : ".Warm"),
                            twaDuration,
                            50,
                            DateUtils.MINUTE_IN_MILLIS,
                            50);
                    RecordHistogram.recordSparseHistogram(
                            "CustomTabs.AndroidBrowserHelper.Version", mBrowserHelperVersion);
                    String metricName =
                            "TimeToFirstCommitNavigation2." + (isTwaColdStart() ? "Cold" : "Warm");
                    new UkmRecorder(webContents, "TrustedWebActivity.Startup")
                            .addMetric(metricName, (int) twaDuration)
                            .addMetric("AndroidBrowserHelper.Version", mBrowserHelperVersion)
                            .record();
                });
    }

    private void recordFirstContentfulPaint(WebContents webContents, long fcpUptimeMillis) {
        recordPaint(webContents, fcpUptimeMillis, "TimeToFirstContentfulPaint");
    }

    private void recordLargestContentfulPaint(WebContents webContents, long lcpUptimeMillis) {
        recordPaint(webContents, lcpUptimeMillis, "TimeToLargestContentfulPaint2");
    }

    private boolean isTwaColdStart() {
        // TWAs usually start by initiating a CustomTabsConnection first, which
        // means we will get ProcessCreationReason.SERVICE as the startup
        // reason all the time.
        //
        // Since that would not be useful, check for the timestamp from the TWA
        // wrapper instead. If the wrapper started before the browser process,
        // treat it as a cold start, no matter whatever warm up or other
        // initialization that the wrapper could perform in between.
        return mTwaStartupUptimeMillis != 0
                && mTwaStartupUptimeMillis < Process.getStartUptimeMillis();
    }

    private void recordPaint(
            WebContents webContents, long paintUptimeMillis, String paintMetricName) {
        if (mCustomTabsConnection == null) return;
        String suffix = null;
        long duration = 0;
        // Note that this will exclude Webapp launches in all cases due to either
        // mUsedHiddenTabSpeculation being null, or mIntentReceivedTimestamp being 0.
        if (mUsedHiddenTabSpeculation != null && mUsedHiddenTabSpeculation) {
            duration = paintUptimeMillis - mLaunchedForSpeculationUptimeMillis;
            suffix = ".Speculated";
        } else if (mIntentReceivedRealtimeMillis > 0) {
            // When the process is already warm the earliest measurable point in startup is when the
            // intent is received so we measure from there. In the cold start case we measure from
            // when the process was started as the best comparison against the warm case.
            if (wasWarmedUp()) {
                duration = paintUptimeMillis - mIntentReceivedUptimeMillis;
                suffix = ".WarmedUp";
            } else if (ColdStartTracker.wasColdOnFirstActivityCreationOrNow()
                    && SimpleStartupForegroundSessionDetector.runningCleanForegroundSession()) {
                duration = paintUptimeMillis - Process.getStartUptimeMillis();
                suffix = ".Cold";
            } else {
                duration = paintUptimeMillis - mIntentReceivedUptimeMillis;
                suffix = ".Warm";
            }
        }
        if (suffix != null) {
            RecordHistogram.recordCustomTimesHistogram(
                    "CustomTabs.Startup." + paintMetricName + suffix,
                    duration,
                    50,
                    DateUtils.MINUTE_IN_MILLIS,
                    50);
        }
        callOnTwaStartupTimeAvailable(
                () -> {
                    long twaDuration = mFirstCommitUptimeMillis - mTwaStartupUptimeMillis;
                    // The TWA durations are always relative to the startup time passed in the
                    // Intent.
                    RecordHistogram.recordCustomTimesHistogram(
                            "TrustedWebActivity.Startup."
                                    + paintMetricName
                                    + (isTwaColdStart() ? ".Cold" : ".Warm"),
                            twaDuration,
                            50,
                            DateUtils.MINUTE_IN_MILLIS,
                            50);
                    String metricName =
                            paintMetricName + "." + (isTwaColdStart() ? "Cold" : "Warm");
                    new UkmRecorder(webContents, "TrustedWebActivity.Startup")
                            .addMetric(metricName, (int) twaDuration)
                            .record();
                });
    }

    private void recordUserTiming(
            WebContents webContents, long markUptimeMillis, String markMetricName) {
        if (mCustomTabsConnection == null) return;
        callOnTwaStartupTimeAvailable(
                () -> {
                    long twaDuration = markUptimeMillis - mTwaStartupUptimeMillis;
                    String metricName = markMetricName + "." + (isTwaColdStart() ? "Cold" : "Warm");
                    new UkmRecorder(webContents, "TrustedWebActivity.Startup")
                            .addMetric(metricName, (int) twaDuration)
                            .record();
                });
    }

    @Override
    public void onDestroyed(Tab tab) {
        TabInteractionRecorder observer = TabInteractionRecorder.getFromTab(tab);
        if (observer != null) observer.onTabClosing();
    }

    @Override
    public void onShown(Tab tab, int type) {
        TabInteractionRecorder.createForTab(tab);
    }

    public void onFirstMeaningfulPaint(Tab tab) {
        mNavigationInfoCaptureTrigger.onFirstMeaningfulPaint(tab);
    }

    private void resetPageLoadTracking() {
        mCurrentState = State.RESET;
    }

    private void captureNavigationInfo(final Tab tab) {
        if (mCustomTabsConnection == null) return;
        if (!mCustomTabsConnection.shouldSendNavigationInfoForSession(mSession)) return;
        if (tab.getWebContents() == null) return;
        String title = tab.getTitle();
        if (TextUtils.isEmpty(title)) return;
        mCustomTabsConnection.sendNavigationInfo(mSession, tab.getUrl().getSpec(), title, null);
    }

    private void callOnTwaStartupTimeAvailable(Runnable callback) {
        if (mTwaStartupUptimeMillis != 0) {
            callback.run();
        } else {
            mTwaStartupTimeAvailableCallbacks.add(callback);
        }
    }

    public void setTwaStartupMetadata(int browserHelperVersion, long startupUptimeMillis) {
        // Support for both were added in 2.6.1 (android-browser-helper version code 1), so we
        // either get both or neither.
        if (browserHelperVersion == 0 || startupUptimeMillis == 0) return;
        assert mBrowserHelperVersion == 0;
        mBrowserHelperVersion = browserHelperVersion;
        assert mTwaStartupUptimeMillis == 0;
        mTwaStartupUptimeMillis = startupUptimeMillis;
        for (Runnable callback : mTwaStartupTimeAvailableCallbacks) {
            callback.run();
        }
        mTwaStartupTimeAvailableCallbacks.clear();
    }
}
