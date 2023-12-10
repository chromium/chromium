// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.APP_CONTEXT;

import android.content.Context;
import android.content.Intent;
import android.graphics.Rect;
import android.net.Uri;
import android.os.Process;
import android.os.SystemClock;
import android.text.TextUtils;
import android.text.format.DateUtils;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabsSessionToken;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.base.ColdStartTracker;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.ClientManager.CalledWarmup;
import org.chromium.chrome.browser.customtabs.features.TabInteractionRecorder;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.intents.BrowserIntentUtils;
import org.chromium.chrome.browser.metrics.SimpleStartupForegroundSessionDetector;
import org.chromium.chrome.browser.page_load_metrics.PageLoadMetrics;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.components.browser_ui.share.ShareImageFileUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

import javax.inject.Inject;
import javax.inject.Named;

/** A {@link TabObserver} that also handles custom tabs specific logging and messaging. */
@ActivityScope
public class CustomTabObserver extends EmptyTabObserver {
    private final CustomTabsConnection mCustomTabsConnection;
    private final CustomTabsSessionToken mSession;
    private final boolean mOpenedByChrome;
    private final NavigationInfoCaptureTrigger mNavigationInfoCaptureTrigger =
            new NavigationInfoCaptureTrigger(this::captureNavigationInfo);
    private int mContentBitmapWidth;
    private int mContentBitmapHeight;

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
    @Nullable private Boolean mUsedHiddenTabSpeculation;

    // The time page load started in the most recent Custom Tab launch.
    private long mPageLoadStartedRealtimeMillis;

    // The time of the first navigation commit in the most recent Custom Tab launch.
    private long mFirstCommitRealtimeMillis;

    @IntDef({State.RESET, State.WAITING_LOAD_START, State.WAITING_LOAD_FINISH})
    @Retention(RetentionPolicy.SOURCE)
    @interface State {
        int RESET = 0;
        int WAITING_LOAD_START = 1;
        int WAITING_LOAD_FINISH = 2;
    }

    // Tracks what point in the first navigation after a Custom Tab launch we're in.
    private @State int mCurrentState;

    private LargestContentfulPaintObserver mLCPObserver;

    private class LargestContentfulPaintObserver implements PageLoadMetrics.Observer {
        @Override
        public void onLargestContentfulPaint(
                WebContents webContents,
                long navigationId,
                long navigationStartMicros,
                long largestContentfulPaintMs,
                long largestContentfulPaintSize) {
            recordLargestContentfulPaint(navigationStartMicros / 1000 + largestContentfulPaintMs);
            PageLoadMetrics.removeObserver(mLCPObserver);
            mLCPObserver = null;
        }
    }

    @Inject
    public CustomTabObserver(
            @Named(APP_CONTEXT) Context appContext,
            BrowserServicesIntentDataProvider intentDataProvider,
            CustomTabsConnection connection) {
        mOpenedByChrome = intentDataProvider.isOpenedByChrome();
        mCustomTabsConnection = mOpenedByChrome ? null : connection;
        mSession = intentDataProvider.getSession();
        if (!mOpenedByChrome
                && mCustomTabsConnection.shouldSendNavigationInfoForSession(mSession)
                && !mCustomTabsConnection.isCCTAPIDeprecated("bitmap")) {
            float desiredWidth =
                    appContext
                            .getResources()
                            .getDimensionPixelSize(R.dimen.custom_tabs_screenshot_width);
            float desiredHeight =
                    appContext
                            .getResources()
                            .getDimensionPixelSize(R.dimen.custom_tabs_screenshot_height);
            Rect bounds = TabUtils.estimateContentSize(appContext);
            if (bounds.width() == 0 || bounds.height() == 0) {
                mContentBitmapWidth = Math.round(desiredWidth);
                mContentBitmapHeight = Math.round(desiredHeight);
            } else {
                // Compute a size that scales the content bitmap to fit one (or both) dimensions,
                // but also preserves aspect ratio.
                float scale =
                        Math.min(desiredWidth / bounds.width(), desiredHeight / bounds.height());
                mContentBitmapWidth = Math.round(bounds.width() * scale);
                mContentBitmapHeight = Math.round(bounds.height() * scale);
            }
        }
        resetPageLoadTracking();
    }

    private void trackNextLCP() {
        if (mLCPObserver != null) return;
        mLCPObserver = new LargestContentfulPaintObserver();
        PageLoadMetrics.addObserver(mLCPObserver, true);
    }

    /**
     * Tracks the next page load, with timestamp as the origin of time.
     * If a load is already happening, we track its PLT.
     * If not, we track NavigationCommit timing + PLT for the next load.
     */
    public void trackNextPageLoadForLaunch(Tab tab, Intent sourceIntent) {
        mIntentReceivedRealtimeMillis = BrowserIntentUtils.getStartupRealtimeMillis(sourceIntent);
        mIntentReceivedUptimeMillis = BrowserIntentUtils.getStartupUptimeMillis(sourceIntent);
        if (tab.isLoading()) {
            mPageLoadStartedRealtimeMillis = -1;
            mCurrentState = State.WAITING_LOAD_FINISH;
        } else {
            mCurrentState = State.WAITING_LOAD_START;
        }
        trackNextLCP();
    }

    public void trackNextPageLoadForHiddenTab(
            boolean usedSpeculation, boolean hasCommitted, Intent sourceIntent) {
        mUsedHiddenTabSpeculation = usedSpeculation;
        mLaunchedForSpeculationRealtimeMillis =
                BrowserIntentUtils.getStartupRealtimeMillis(sourceIntent);
        mLaunchedForSpeculationUptimeMillis =
                BrowserIntentUtils.getStartupUptimeMillis(sourceIntent);
        trackNextLCP();
        if (usedSpeculation && hasCommitted) {
            recordFirstCommitNavigation(mLaunchedForSpeculationRealtimeMillis);
        }
    }

    @Override
    public void onLoadUrl(Tab tab, LoadUrlParams params, int loadType) {
        if (mCustomTabsConnection != null) {
            mCustomTabsConnection.registerLaunch(mSession, params.getUrl());
        }
    }

    @Override
    public void onPageLoadStarted(Tab tab, GURL url) {
        if (mCurrentState == State.WAITING_LOAD_START) {
            mPageLoadStartedRealtimeMillis = SystemClock.elapsedRealtime();
            mCurrentState = State.WAITING_LOAD_FINISH;
        } else if (mCurrentState == State.WAITING_LOAD_FINISH) {
            if (mCustomTabsConnection != null) {
                mCustomTabsConnection.sendNavigationInfo(
                        mSession, tab.getUrl().getSpec(), tab.getTitle(), (Uri) null);
            }
            mPageLoadStartedRealtimeMillis = SystemClock.elapsedRealtime();
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
        long pageLoadFinishedTimestamp = SystemClock.elapsedRealtime();

        if (mCurrentState == State.WAITING_LOAD_FINISH && mIntentReceivedRealtimeMillis > 0) {
            String histogramPrefix = mOpenedByChrome ? "ChromeGeneratedCustomTab" : "CustomTabs";
            long timeToPageLoadFinishedMs =
                    pageLoadFinishedTimestamp - mIntentReceivedRealtimeMillis;
            if (mPageLoadStartedRealtimeMillis > 0) {
                long timeToPageLoadStartedMs =
                        mPageLoadStartedRealtimeMillis - mIntentReceivedRealtimeMillis;
                // Intent to Load Start is recorded here to make sure we do not record
                // failed/aborted page loads.
                RecordHistogram.recordCustomTimesHistogram(
                        histogramPrefix + ".IntentToFirstNavigationStartTime.ZoomedOut",
                        timeToPageLoadStartedMs,
                        50,
                        DateUtils.MINUTE_IN_MILLIS * 10,
                        50);
                RecordHistogram.recordCustomTimesHistogram(
                        histogramPrefix + ".IntentToFirstNavigationStartTime.ZoomedIn",
                        timeToPageLoadStartedMs,
                        200,
                        DateUtils.SECOND_IN_MILLIS,
                        100);
            }
            // Same bounds and bucket count as PLT histograms.
            RecordHistogram.recordCustomTimesHistogram(
                    histogramPrefix + ".IntentToPageLoadedTime",
                    timeToPageLoadFinishedMs,
                    10,
                    DateUtils.MINUTE_IN_MILLIS * 10,
                    100);

            // Not all page loads go through a navigation commit (prerender for instance).
            if (mPageLoadStartedRealtimeMillis != 0) {
                long timeToFirstCommitMs =
                        mFirstCommitRealtimeMillis - mIntentReceivedRealtimeMillis;
                // Current median is 550ms, and long tail is very long. ZoomedIn gives good view of
                // the median and ZoomedOut gives a good overview.
                RecordHistogram.recordCustomTimesHistogram(
                        "CustomTabs.IntentToFirstCommitNavigationTime3.ZoomedIn",
                        timeToFirstCommitMs,
                        200,
                        DateUtils.SECOND_IN_MILLIS,
                        100);
                // For ZoomedOut very rarely is it under 50ms and this range matches
                // CustomTabs.IntentToFirstCommitNavigationTime2.ZoomedOut.
                RecordHistogram.recordCustomTimesHistogram(
                        "CustomTabs.IntentToFirstCommitNavigationTime3.ZoomedOut",
                        timeToFirstCommitMs,
                        50,
                        DateUtils.MINUTE_IN_MILLIS * 10,
                        50);
            }
        }
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

        recordFirstCommitNavigation(mFirstCommitRealtimeMillis);
    }

    private void recordFirstCommitNavigation(long firstCommitRealtimeMillis) {
        if (mCustomTabsConnection == null) return;
        String histogram = null;
        long duration = 0;
        // Note that this will exclude Webapp launches in all cases due to either
        // mUsedHiddenTabSpeculation being null, or mIntentReceivedTimestamp being 0.
        if (mUsedHiddenTabSpeculation != null && mUsedHiddenTabSpeculation) {
            duration = firstCommitRealtimeMillis - mLaunchedForSpeculationRealtimeMillis;
            histogram = "CustomTabs.Startup.TimeToFirstCommitNavigation2.Speculated";
        } else if (mIntentReceivedRealtimeMillis > 0) {
            // When the process is already warm the earliest measurable point in startup is when the
            // intent is received so we measure from there. In the cold start case we measure from
            // when the process was started as the best comparison against the warm case.
            if (wasWarmedUp()) {
                duration = firstCommitRealtimeMillis - mIntentReceivedRealtimeMillis;
                histogram = "CustomTabs.Startup.TimeToFirstCommitNavigation2.WarmedUp";
            } else if (ColdStartTracker.wasColdOnFirstActivityCreationOrNow()
                    && SimpleStartupForegroundSessionDetector.runningCleanForegroundSession()) {
                duration = firstCommitRealtimeMillis - Process.getStartElapsedRealtime();
                histogram = "CustomTabs.Startup.TimeToFirstCommitNavigation2.Cold";
            } else {
                duration = firstCommitRealtimeMillis - mIntentReceivedRealtimeMillis;
                histogram = "CustomTabs.Startup.TimeToFirstCommitNavigation2.Warm";
            }
        }
        if (histogram != null) {
            RecordHistogram.recordCustomTimesHistogram(
                    histogram, duration, 50, DateUtils.MINUTE_IN_MILLIS, 50);
        }
    }

    public void recordLargestContentfulPaint(long lcpUptimeMillis) {
        if (mCustomTabsConnection == null) return;
        String histogram = null;
        long duration = 0;
        // Note that this will exclude Webapp launches in all cases due to either
        // mUsedHiddenTabSpeculation being null, or mIntentReceivedTimestamp being 0.
        if (mUsedHiddenTabSpeculation != null && mUsedHiddenTabSpeculation) {
            duration = lcpUptimeMillis - mLaunchedForSpeculationUptimeMillis;
            histogram = "CustomTabs.Startup.TimeToLargestContentfulPaint2.Speculated";
        } else if (mIntentReceivedRealtimeMillis > 0) {
            // When the process is already warm the earliest measurable point in startup is when the
            // intent is received so we measure from there. In the cold start case we measure from
            // when the process was started as the best comparison against the warm case.
            if (wasWarmedUp()) {
                duration = lcpUptimeMillis - mIntentReceivedUptimeMillis;
                histogram = "CustomTabs.Startup.TimeToLargestContentfulPaint2.WarmedUp";
            } else if (ColdStartTracker.wasColdOnFirstActivityCreationOrNow()
                    && SimpleStartupForegroundSessionDetector.runningCleanForegroundSession()) {
                duration = lcpUptimeMillis - Process.getStartUptimeMillis();
                histogram = "CustomTabs.Startup.TimeToLargestContentfulPaint2.Cold";
            } else {
                duration = lcpUptimeMillis - mIntentReceivedUptimeMillis;
                histogram = "CustomTabs.Startup.TimeToLargestContentfulPaint2.Warm";
            }
        }
        if (histogram != null) {
            RecordHistogram.recordCustomTimesHistogram(
                    histogram, duration, 50, DateUtils.MINUTE_IN_MILLIS, 50);
        }
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
        String urlString = tab.getUrl().getSpec();

        if (mCustomTabsConnection.isCCTAPIDeprecated("bitmap")) {
            mCustomTabsConnection.sendNavigationInfo(mSession, urlString, title, null);
        } else {
            ShareImageFileUtils.captureScreenshotForContents(
                    tab.getWebContents(),
                    mContentBitmapWidth,
                    mContentBitmapHeight,
                    (Uri snapshotPath) -> {
                        if (snapshotPath == null) return;
                        mCustomTabsConnection.sendNavigationInfo(
                                mSession, urlString, title, snapshotPath);
                    });
        }
    }
}
