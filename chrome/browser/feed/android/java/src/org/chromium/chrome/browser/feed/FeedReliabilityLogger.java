// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.os.SystemClock;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.xsurface.feed.FeedCardOpeningReliabilityLogger;
import org.chromium.chrome.browser.xsurface.feed.FeedCardOpeningReliabilityLogger.PageLoadError;
import org.chromium.chrome.browser.xsurface.feed.FeedLaunchReliabilityLogger;
import org.chromium.chrome.browser.xsurface.feed.FeedUserInteractionReliabilityLogger;
import org.chromium.chrome.browser.xsurface.feed.FeedUserInteractionReliabilityLogger.ClosedReason;
import org.chromium.chrome.browser.xsurface.feed.StreamType;
import org.chromium.components.feed.proto.wire.ReliabilityLoggingEnums.DiscoverLaunchResult;
import org.chromium.net.NetError;

/** Home for logic related to feed reliability logging. */
public class FeedReliabilityLogger implements UrlFocusChangeListener {
    private final FeedLaunchReliabilityLogger mLaunchLogger;
    private final @Nullable FeedUserInteractionReliabilityLogger mUserInteractionLogger;
    private final FeedCardOpeningReliabilityLogger mCardOpeningLogger;

    /**
     * Constructor records some info known about the feed UI before mLaunchLogger is available. UI
     * surface type and creation timestamp are logged as part of the feed launch flow.
     *
     * @param launchLogger FeedLaunchReliabilityLogger for recording events during feed loading.
     * @param userInteractionLogger FeedUserInteractionReliabilityLogger for tracking user
     *     interaction with feed content.
     * @param cardOpeningLogger FeedCardOpeningLogger for report events related to card tapping.
     */
    public FeedReliabilityLogger(
            @NonNull FeedLaunchReliabilityLogger launchLogger,
            @Nullable FeedUserInteractionReliabilityLogger userInteractionLogger,
            @NonNull FeedCardOpeningReliabilityLogger cardOpeningLogger) {
        mLaunchLogger = launchLogger;
        mUserInteractionLogger = userInteractionLogger;
        mCardOpeningLogger = cardOpeningLogger;
    }

    /** Call this when the activity is paused. */
    public void onActivityPaused() {
        logLaunchFinishedIfInProgress(
                DiscoverLaunchResult.FRAGMENT_PAUSED, /* userMightComeBack= */ false);
    }

    /** Call this when the activity is resumed. */
    public void onActivityResumed() {
        mLaunchLogger.cancelPendingFinished();
    }

    /** Call this when the user focuses the omnibox. */
    public void onOmniboxFocused() {
        // The user could return to the feed while it's still loading, so consider the launch
        // "pending finished".
        logLaunchFinishedIfInProgress(
                DiscoverLaunchResult.SEARCH_BOX_TAPPED, /* userMightComeBack= */ true);
    }

    /** Call this when the user performs a voice search. */
    public void onVoiceSearch() {
        // The user could return to the feed while it's still loading, so consider the launch
        // "pending finished".
        logLaunchFinishedIfInProgress(
                DiscoverLaunchResult.VOICE_SEARCH_TAPPED, /* userMightComeBack= */ true);
    }

    /**
     * Call this when the user has navigated to a webpage. If it was a card tap, instead use
     * CARD_TAPPED.
     */
    public void onPageLoadStarted() {
        logLaunchFinishedIfInProgress(
                DiscoverLaunchResult.NAVIGATED_AWAY_IN_APP, /* userMightComeBack= */ false);
    }

    /**
     * Call this when the user has pressed the back button and it will cause the feed to disappear.
     */
    public void onNavigateBack() {
        logLaunchFinishedIfInProgress(
                DiscoverLaunchResult.NAVIGATED_BACK, /* userMightComeBack= */ false);
    }

    /** Call this when the user selects a tab. */
    public void onSwitchTabs() {
        logLaunchFinishedIfInProgress(
                DiscoverLaunchResult.NAVIGATED_TO_ANOTHER_TAB, /* userMightComeBack= */ false);
    }

    /** Call this when the user switches to another stream. */
    public void onSwitchStream(@StreamType int switchedToStream) {
        logLaunchFinishedIfInProgress(
                DiscoverLaunchResult.SWITCHED_FEED_TABS, /* userMightComeBack= */ false);
        mLaunchLogger.logSwitchedFeeds(switchedToStream, SystemClock.elapsedRealtimeNanos());
    }

    /** Call this when the stream is binded. */
    public void onBindStream(@StreamType int streamType, int streamId) {
        mLaunchLogger.sendPendingEvents(streamType, streamId);
        mLaunchLogger.logFeedReloading(System.nanoTime());

        if (mUserInteractionLogger != null) {
            mUserInteractionLogger.onStreamOpened(streamType);
        }
    }

    /** Call this when the stream is unbinded. */
    public void onUnbindStream(@ClosedReason int closedReason) {
        logLaunchFinishedIfInProgress(
                DiscoverLaunchResult.FRAGMENT_STOPPED, /* userMightComeBack= */ false);
        reportStreamClosed(closedReason);
    }

    /** Call this when the card is about to open. */
    public void onOpenCard(int pageId, int cardCategory) {
        logLaunchFinishedIfInProgress(
                DiscoverLaunchResult.CARD_TAPPED, /* userMightComeBack= */ false);
        mCardOpeningLogger.onCardClicked(pageId, cardCategory);
    }

    /** Call this when the page starts loading. */
    public void onPageLoadStarted(int pageId) {
        mCardOpeningLogger.onPageLoadStarted(pageId);
    }

    /** Call this when the page finishes loading. */
    public void onPageLoadFinished(int pageId) {
        mCardOpeningLogger.onPageLoadFinished(pageId);
    }

    /** Call this when the page fails to load. */
    public void onPageLoadFailed(int pageId, @NetError int errorCode) {
        int pageLoadError;
        switch (errorCode) {
            case NetError.ERR_INTERNET_DISCONNECTED:
                pageLoadError = PageLoadError.INTERNET_DISCONNECTED;
                break;
            case NetError.ERR_CONNECTION_TIMED_OUT:
                pageLoadError = PageLoadError.CONNECTION_TIMED_OUT;
                break;
            case NetError.ERR_NAME_RESOLUTION_FAILED:
                pageLoadError = PageLoadError.NAME_RESOLUTION_FAILED;
                break;
            default:
                pageLoadError = PageLoadError.PAGE_LOAD_ERROR;
                break;
        }
        mCardOpeningLogger.onPageLoadFailed(pageId, pageLoadError);
    }

    /** Called when the page finishes first paint after non-empty layout. */
    public void onPageFirstContentfulPaint(int pageId) {
        mCardOpeningLogger.onPageFirstContentfulPaint(pageId);
    }

    /** Call this when the view is barely visible for the first time. */
    public void onViewFirstVisible(View view) {
        if (mUserInteractionLogger != null) {
            mUserInteractionLogger.onViewFirstVisible(view);
        }
    }

    /** Call this when the view is rendered for the first time. */
    public void onViewFirstRendered(View view) {
        if (mUserInteractionLogger != null) {
            mUserInteractionLogger.onViewFirstRendered(view);
        }
    }

    /** Call this when the loading indicator for load-more is shown. */
    public void onPaginationIndicatorShown() {
        if (mUserInteractionLogger != null) {
            mUserInteractionLogger.onPaginationIndicatorShown();
        }
    }

    /** Call this when the user scrolled away from the loading indicator for load-more. */
    public void onPaginationUserScrolledAwayFromIndicator() {
        if (mUserInteractionLogger != null) {
            mUserInteractionLogger.onPaginationUserScrolledAwayFromIndicator();
        }
    }

    // UrlFocusChangeListener

    @Override
    public void onUrlFocusChange(boolean hasFocus) {
        // URL bar gaining focus is already handled by onOmniboxFocused() and onVoiceSearch(). We
        // just care about when it loses focus while the feed is still loading.
        if (hasFocus || !mLaunchLogger.isLaunchInProgress()) {
            return;
        }
        mLaunchLogger.cancelPendingFinished();
    }

    @Override
    public void onUrlAnimationFinished(boolean hasFocus) {}

    /** Get the {@link FeedLaunchReliabilityLogger}. */
    public FeedLaunchReliabilityLogger getLaunchLogger() {
        return mLaunchLogger;
    }

    /** Get the {@link FeedUserInteractionReliabilityLogger}. May be null if not enabled. */
    public @Nullable FeedUserInteractionReliabilityLogger getUserInteractionLogger() {
        return mUserInteractionLogger;
    }

    /**
     * Log that the feed launch has finished. Does nothing if the feed wasn't launching.
     * @param status DiscoverLaunchResult indicating how the launch ended.
     * @param userMightComeBack Whether to treat the end of the launch as tentative: true if the
     *         user could return to the feed while it's still loading, false otherwise.
     */
    private void logLaunchFinishedIfInProgress(
            DiscoverLaunchResult status, boolean userMightComeBack) {
        if (!mLaunchLogger.isLaunchInProgress()) {
            return;
        }
        if (userMightComeBack) {
            mLaunchLogger.pendingFinished(now(), status.getNumber());
        } else {
            mLaunchLogger.logLaunchFinished(now(), status.getNumber());
        }
    }

    private void reportStreamClosed(@ClosedReason int closedReason) {
        if (mUserInteractionLogger != null) {
            mUserInteractionLogger.onStreamClosed(closedReason);
        }
    }

    private static long now() {
        return SystemClock.elapsedRealtimeNanos();
    }
}
