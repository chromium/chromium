// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.os.SystemClock;

import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.xsurface.FeedLaunchReliabilityLogger;
import org.chromium.components.feed.proto.wire.ReliabilityLoggingEnums.DiscoverLaunchResult;

/** Home for logic related to feed reliability logging. */
public class FeedReliabilityLogger implements UrlFocusChangeListener {
    private final FeedLaunchReliabilityLogger mLaunchLogger;

    /**
     * Constructor records some info known about the feed UI before mLaunchLogger is available. UI
     * surface type and creation timestamp are logged as part of the feed launch flow.
     * @param launchLogger FeedLaunchReliabilityLogger for recording events during feed loading.
     */
    public FeedReliabilityLogger(FeedLaunchReliabilityLogger launchLogger) {
        mLaunchLogger = launchLogger;
    }

    /** Call this when the activity is paused. */
    public void onActivityPaused() {
        logLaunchFinishedIfInProgress(
                DiscoverLaunchResult.FRAGMENT_PAUSED, /*userMightComeBack=*/false);
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
                DiscoverLaunchResult.SEARCH_BOX_TAPPED, /*userMightComeBack=*/true);
    }

    /** Call this when the user performs a voice search. */
    public void onVoiceSearch() {
        // The user could return to the feed while it's still loading, so consider the launch
        // "pending finished".
        logLaunchFinishedIfInProgress(
                DiscoverLaunchResult.VOICE_SEARCH_TAPPED, /*userMightComeBack=*/true);
    }

    /**
     * Call this when the user has navigated to a webpage. If it was a card tap, instead use
     * CARD_TAPPED.
     */
    public void onPageLoadStarted() {
        logLaunchFinishedIfInProgress(
                DiscoverLaunchResult.NAVIGATED_AWAY_IN_APP, /*userMightComeBack=*/false);
    }

    /**
     * Call this when the user has pressed the back button and it will cause the feed to disappear.
     */
    public void onNavigateBack() {
        logLaunchFinishedIfInProgress(
                DiscoverLaunchResult.NAVIGATED_BACK, /*userMightComeBack=*/false);
    }

    /** Call this when the user selects a tab. */
    public void onSwitchTabs() {
        logLaunchFinishedIfInProgress(DiscoverLaunchResult.NAVIGATED_TO_ANOTHER_TAB,
                /*userMightComeBack=*/false);
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

    /** Get the {@link FeedLaunchReliabilityLogger}. May not return the same instance every time. */
    public FeedLaunchReliabilityLogger getLaunchLogger() {
        return mLaunchLogger;
    }

    /**
     * Log that the feed launch has finished. Does nothing if the feed wasn't launching.
     * @param status DiscoverLaunchResult indicating how the launch ended.
     * @param userMightComeBack Whether to treat the end of the launch as tentative: true if the
     *         user could return to the feed while it's still loading, false otherwise.
     */
    public void logLaunchFinishedIfInProgress(
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

    private static long now() {
        return SystemClock.elapsedRealtimeNanos();
    }
}