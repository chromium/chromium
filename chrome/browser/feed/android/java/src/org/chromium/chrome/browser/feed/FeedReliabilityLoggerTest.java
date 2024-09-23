// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.xsurface.feed.FeedCardOpeningReliabilityLogger;
import org.chromium.chrome.browser.xsurface.feed.FeedCardOpeningReliabilityLogger.PageLoadError;
import org.chromium.chrome.browser.xsurface.feed.FeedLaunchReliabilityLogger;
import org.chromium.chrome.browser.xsurface.feed.FeedLaunchReliabilityLogger.StreamType;
import org.chromium.chrome.browser.xsurface.feed.FeedUserInteractionReliabilityLogger;
import org.chromium.chrome.browser.xsurface.feed.FeedUserInteractionReliabilityLogger.ClosedReason;
import org.chromium.components.feed.proto.wire.ReliabilityLoggingEnums.DiscoverLaunchResult;
import org.chromium.net.NetError;

/** Unit tests for {@link FeedReliabilityLogger}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FeedReliabilityLoggerTest {
    static final int CARD_CATEGORY = 101;
    static final int PAGE_ID = 5;

    @Mock FeedLaunchReliabilityLogger mLaunchLogger;
    @Mock FeedUserInteractionReliabilityLogger mUserInteractionLogger;
    @Mock FeedCardOpeningReliabilityLogger mCardOpeningReliabilityLogger;

    FeedReliabilityLogger mFeedReliabilityLogger;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mFeedReliabilityLogger =
                new FeedReliabilityLogger(
                        mLaunchLogger, mUserInteractionLogger, mCardOpeningReliabilityLogger);
    }

    @Test
    public void testOnActivityPaused_launchInProgress() {
        when(mLaunchLogger.isLaunchInProgress()).thenReturn(true);

        mFeedReliabilityLogger.onActivityPaused();
        verify(mLaunchLogger, times(1))
                .logLaunchFinished(anyLong(), eq(DiscoverLaunchResult.FRAGMENT_PAUSED.getNumber()));
    }

    @Test
    public void testOnActivityPaused_launchNotInProgress() {
        when(mLaunchLogger.isLaunchInProgress()).thenReturn(false);

        mFeedReliabilityLogger.onActivityPaused();
        verify(mLaunchLogger, never()).logLaunchFinished(anyLong(), anyInt());
    }

    @Test
    public void testOnActivityResumed() {
        mFeedReliabilityLogger.onActivityResumed();
        verify(mLaunchLogger, times(1)).cancelPendingFinished();
    }

    @Test
    public void testOnOmniboxFocused() {
        when(mLaunchLogger.isLaunchInProgress()).thenReturn(true);
        mFeedReliabilityLogger.onOmniboxFocused();
        verify(mLaunchLogger, times(1))
                .pendingFinished(anyLong(), eq(DiscoverLaunchResult.SEARCH_BOX_TAPPED.getNumber()));
    }

    @Test
    public void testOnVoiceSearch() {
        when(mLaunchLogger.isLaunchInProgress()).thenReturn(true);
        mFeedReliabilityLogger.onVoiceSearch();
        verify(mLaunchLogger, times(1))
                .pendingFinished(
                        anyLong(), eq(DiscoverLaunchResult.VOICE_SEARCH_TAPPED.getNumber()));
    }

    @Test
    public void testOnUrlFocusChange_gainFocus_launchInProgress() {
        when(mLaunchLogger.isLaunchInProgress()).thenReturn(true);
        mFeedReliabilityLogger.onUrlFocusChange(/* hasFocus= */ true);
        verify(mLaunchLogger, never()).cancelPendingFinished();
    }

    @Test
    public void testOnUrlFocusChange_gainFocus_launchNotInProgress() {
        when(mLaunchLogger.isLaunchInProgress()).thenReturn(false);
        mFeedReliabilityLogger.onUrlFocusChange(/* hasFocus= */ true);
        verify(mLaunchLogger, never()).cancelPendingFinished();
    }

    @Test
    public void testOnUrlFocusChange_loseFocus_launchInProgress() {
        when(mLaunchLogger.isLaunchInProgress()).thenReturn(true);
        mFeedReliabilityLogger.onUrlFocusChange(/* hasFocus= */ false);
        verify(mLaunchLogger, times(1)).cancelPendingFinished();
    }

    @Test
    public void testOnUrlFocusChange_loseFocus_launchNotInProgress() {
        when(mLaunchLogger.isLaunchInProgress()).thenReturn(false);
        mFeedReliabilityLogger.onUrlFocusChange(/* hasFocus= */ false);
        verify(mLaunchLogger, never()).cancelPendingFinished();
    }

    @Test
    public void testOnPageLoadStarted() {
        when(mLaunchLogger.isLaunchInProgress()).thenReturn(true);
        mFeedReliabilityLogger.onPageLoadStarted();
        verify(mLaunchLogger)
                .logLaunchFinished(
                        anyLong(), eq(DiscoverLaunchResult.NAVIGATED_AWAY_IN_APP.getNumber()));
    }

    @Test
    public void testOnNavigateBack() {
        when(mLaunchLogger.isLaunchInProgress()).thenReturn(true);
        mFeedReliabilityLogger.onNavigateBack();
        verify(mLaunchLogger)
                .logLaunchFinished(anyLong(), eq(DiscoverLaunchResult.NAVIGATED_BACK.getNumber()));
    }

    @Test
    public void testOnSwitchTabs() {
        when(mLaunchLogger.isLaunchInProgress()).thenReturn(true);
        mFeedReliabilityLogger.onSwitchTabs();
        verify(mLaunchLogger)
                .logLaunchFinished(
                        anyLong(), eq(DiscoverLaunchResult.NAVIGATED_TO_ANOTHER_TAB.getNumber()));
    }

    @Test
    public void testOnSwitchStream() {
        when(mLaunchLogger.isLaunchInProgress()).thenReturn(true);
        mFeedReliabilityLogger.onSwitchStream(StreamType.FOR_YOU);
        verify(mLaunchLogger)
                .logLaunchFinished(
                        anyLong(), eq(DiscoverLaunchResult.SWITCHED_FEED_TABS.getNumber()));
        verify(mLaunchLogger).logSwitchedFeeds(eq(StreamType.FOR_YOU), anyLong());
    }

    @Test
    public void testOnBindStream() {
        when(mLaunchLogger.isLaunchInProgress()).thenReturn(true);
        mFeedReliabilityLogger.onBindStream(StreamType.FOR_YOU, 0);
        verify(mLaunchLogger).logFeedReloading(anyLong());
        verify(mUserInteractionLogger).onStreamOpened(anyInt());
    }

    @Test
    public void testOnBindStream_supervisedUserFeed() {
        when(mLaunchLogger.isLaunchInProgress()).thenReturn(true);
        mFeedReliabilityLogger.onBindStream(StreamType.SUPERVISED_USER_FEED, 0);
        verify(mLaunchLogger).logFeedReloading(anyLong());
        verify(mUserInteractionLogger).onStreamOpened(eq(StreamType.SUPERVISED_USER_FEED));
    }

    @Test
    public void testOnUnbindStream() {
        when(mLaunchLogger.isLaunchInProgress()).thenReturn(true);
        mFeedReliabilityLogger.onUnbindStream(ClosedReason.LEAVE_FEED);
        verify(mLaunchLogger)
                .logLaunchFinished(
                        anyLong(), eq(DiscoverLaunchResult.FRAGMENT_STOPPED.getNumber()));
        verify(mUserInteractionLogger).onStreamClosed(eq(ClosedReason.LEAVE_FEED));
    }

    @Test
    public void testOnOpenCard() {
        when(mLaunchLogger.isLaunchInProgress()).thenReturn(true);
        mFeedReliabilityLogger.onOpenCard(PAGE_ID, CARD_CATEGORY);
        verify(mLaunchLogger)
                .logLaunchFinished(anyLong(), eq(DiscoverLaunchResult.CARD_TAPPED.getNumber()));
    }

    @Test
    public void testCardOpeningReliabilityLogger() {
        mFeedReliabilityLogger.onOpenCard(PAGE_ID, CARD_CATEGORY);
        verify(mCardOpeningReliabilityLogger).onCardClicked(eq(PAGE_ID), eq(CARD_CATEGORY));

        mFeedReliabilityLogger.onPageLoadStarted(PAGE_ID);
        verify(mCardOpeningReliabilityLogger).onPageLoadStarted(PAGE_ID);

        mFeedReliabilityLogger.onPageFirstContentfulPaint(PAGE_ID);
        verify(mCardOpeningReliabilityLogger).onPageFirstContentfulPaint(PAGE_ID);

        mFeedReliabilityLogger.onPageLoadFinished(PAGE_ID);
        verify(mCardOpeningReliabilityLogger).onPageLoadFinished(PAGE_ID);

        mFeedReliabilityLogger.onPageLoadFailed(PAGE_ID, NetError.ERR_INTERNET_DISCONNECTED);
        verify(mCardOpeningReliabilityLogger)
                .onPageLoadFailed(PAGE_ID, PageLoadError.INTERNET_DISCONNECTED);

        mFeedReliabilityLogger.onPageLoadFailed(PAGE_ID, NetError.ERR_CONNECTION_TIMED_OUT);
        verify(mCardOpeningReliabilityLogger)
                .onPageLoadFailed(PAGE_ID, PageLoadError.CONNECTION_TIMED_OUT);

        mFeedReliabilityLogger.onPageLoadFailed(PAGE_ID, NetError.ERR_NAME_RESOLUTION_FAILED);
        verify(mCardOpeningReliabilityLogger)
                .onPageLoadFailed(PAGE_ID, PageLoadError.NAME_RESOLUTION_FAILED);

        mFeedReliabilityLogger.onPageLoadFailed(PAGE_ID, NetError.ERR_ABORTED);
        verify(mCardOpeningReliabilityLogger)
                .onPageLoadFailed(PAGE_ID, PageLoadError.PAGE_LOAD_ERROR);
    }
}
