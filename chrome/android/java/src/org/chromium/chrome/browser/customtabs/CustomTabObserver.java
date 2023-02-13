// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.APP_CONTEXT;

import android.content.Context;
import android.graphics.Rect;
import android.net.Uri;
import android.os.SystemClock;
import android.text.TextUtils;
import android.text.format.DateUtils;

import androidx.annotation.IntDef;
import androidx.browser.customtabs.CustomTabsSessionToken;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.features.TabInteractionRecorder;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.components.browser_ui.share.ShareImageFileUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

import javax.inject.Inject;
import javax.inject.Named;

/**
 * A {@link TabObserver} that also handles custom tabs specific logging and messaging.
 */
@ActivityScope
public class CustomTabObserver extends EmptyTabObserver {
    private final CustomTabsConnection mCustomTabsConnection;
    private final CustomTabsSessionToken mSession;
    private final boolean mOpenedByChrome;
    private final NavigationInfoCaptureTrigger mNavigationInfoCaptureTrigger =
            new NavigationInfoCaptureTrigger(this::captureNavigationInfo);
    private int mContentBitmapWidth;
    private int mContentBitmapHeight;

    private long mIntentReceivedTimestamp;
    private long mPageLoadStartedTimestamp;
    private long mFirstCommitTimestamp;

    @IntDef({State.RESET, State.WAITING_LOAD_START, State.WAITING_LOAD_FINISH})
    @Retention(RetentionPolicy.SOURCE)
    @interface State {
        int RESET = 0;
        int WAITING_LOAD_START = 1;
        int WAITING_LOAD_FINISH = 2;
    }

    private @State int mCurrentState;

    @Inject
    public CustomTabObserver(@Named(APP_CONTEXT) Context appContext,
            BrowserServicesIntentDataProvider intentDataProvider, CustomTabsConnection connection) {
        mOpenedByChrome = intentDataProvider.isOpenedByChrome();
        mCustomTabsConnection = mOpenedByChrome ? null : connection;
        mSession = intentDataProvider.getSession();
        if (!mOpenedByChrome
                && mCustomTabsConnection.shouldSendNavigationInfoForSession(mSession)) {
            float desiredWidth = appContext.getResources().getDimensionPixelSize(
                    R.dimen.custom_tabs_screenshot_width);
            float desiredHeight = appContext.getResources().getDimensionPixelSize(
                    R.dimen.custom_tabs_screenshot_height);
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

    /**
     * Tracks the next page load, with timestamp as the origin of time.
     * If a load is already happening, we track its PLT.
     * If not, we track NavigationCommit timing + PLT for the next load.
     */
    public void trackNextPageLoadFromTimestamp(Tab tab, long timestamp) {
        mIntentReceivedTimestamp = timestamp;
        if (tab.isLoading()) {
            mPageLoadStartedTimestamp = -1;
            mCurrentState = State.WAITING_LOAD_FINISH;
        } else {
            mCurrentState = State.WAITING_LOAD_START;
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
            mPageLoadStartedTimestamp = SystemClock.elapsedRealtime();
            mCurrentState = State.WAITING_LOAD_FINISH;
        } else if (mCurrentState == State.WAITING_LOAD_FINISH) {
            if (mCustomTabsConnection != null) {
                mCustomTabsConnection.sendNavigationInfo(
                        mSession, tab.getUrl().getSpec(), tab.getTitle(), (Uri) null);
            }
            mPageLoadStartedTimestamp = SystemClock.elapsedRealtime();
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

        if (mCurrentState == State.WAITING_LOAD_FINISH && mIntentReceivedTimestamp > 0) {
            String histogramPrefix = mOpenedByChrome ? "ChromeGeneratedCustomTab" : "CustomTabs";
            long timeToPageLoadFinishedMs = pageLoadFinishedTimestamp - mIntentReceivedTimestamp;
            if (mPageLoadStartedTimestamp > 0) {
                long timeToPageLoadStartedMs = mPageLoadStartedTimestamp - mIntentReceivedTimestamp;
                // Intent to Load Start is recorded here to make sure we do not record
                // failed/aborted page loads.
                RecordHistogram.recordCustomTimesHistogram(
                        histogramPrefix + ".IntentToFirstNavigationStartTime.ZoomedOut",
                        timeToPageLoadStartedMs, 50, DateUtils.MINUTE_IN_MILLIS * 10, 50);
                RecordHistogram.recordCustomTimesHistogram(
                        histogramPrefix + ".IntentToFirstNavigationStartTime.ZoomedIn",
                        timeToPageLoadStartedMs, 200, DateUtils.SECOND_IN_MILLIS, 100);
            }
            // Same bounds and bucket count as PLT histograms.
            RecordHistogram.recordCustomTimesHistogram(histogramPrefix + ".IntentToPageLoadedTime",
                    timeToPageLoadFinishedMs, 10, DateUtils.MINUTE_IN_MILLIS * 10, 100);

            // Not all page loads go through a navigation commit (prerender for instance).
            if (mPageLoadStartedTimestamp != 0) {
                long timeToFirstCommitMs = mFirstCommitTimestamp - mIntentReceivedTimestamp;
                // Current median is 550ms, and long tail is very long. ZoomedIn gives good view of
                // the median and ZoomedOut gives a good overview.
                RecordHistogram.recordCustomTimesHistogram(
                        "CustomTabs.IntentToFirstCommitNavigationTime3.ZoomedIn",
                        timeToFirstCommitMs, 200, DateUtils.SECOND_IN_MILLIS, 100);
                // For ZoomedOut very rarely is it under 50ms and this range matches
                // CustomTabs.IntentToFirstCommitNavigationTime2.ZoomedOut.
                RecordHistogram.recordCustomTimesHistogram(
                        "CustomTabs.IntentToFirstCommitNavigationTime3.ZoomedOut",
                        timeToFirstCommitMs, 50, DateUtils.MINUTE_IN_MILLIS * 10, 50);
            }
        }
        resetPageLoadTracking();
        mNavigationInfoCaptureTrigger.onLoadFinished(tab);
    }

    @Override
    public void onPageLoadFailed(Tab tab, int errorCode) {
        resetPageLoadTracking();
    }

    @Override
    public void onDidFinishNavigationInPrimaryMainFrame(Tab tab, NavigationHandle navigation) {
        boolean firstNavigation = mFirstCommitTimestamp == 0;
        boolean isFirstMainFrameCommit = firstNavigation && navigation.hasCommitted()
                && !navigation.isErrorPage() && !navigation.isSameDocument();
        if (isFirstMainFrameCommit) mFirstCommitTimestamp = SystemClock.elapsedRealtime();
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
        mIntentReceivedTimestamp = -1;
    }

    private void captureNavigationInfo(final Tab tab) {
        if (mCustomTabsConnection == null) return;
        if (!mCustomTabsConnection.shouldSendNavigationInfoForSession(mSession)) return;
        if (tab.getWebContents() == null) return;
        String title = tab.getTitle();
        if (TextUtils.isEmpty(title)) return;
        String urlString = tab.getUrl().getSpec();

        ShareImageFileUtils.captureScreenshotForContents(tab.getWebContents(), mContentBitmapWidth,
                mContentBitmapHeight, (Uri snapshotPath) -> {
                    if (snapshotPath == null) return;
                    mCustomTabsConnection.sendNavigationInfo(
                            mSession, urlString, title, snapshotPath);
                });
    }
}
