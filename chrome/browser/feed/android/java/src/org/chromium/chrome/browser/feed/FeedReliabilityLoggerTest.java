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
import org.chromium.chrome.browser.xsurface.FeedLaunchReliabilityLogger;
import org.chromium.components.feed.proto.wire.ReliabilityLoggingEnums.DiscoverLaunchResult;

/** Unit tests for {@link FeedReliabilityLogger}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FeedReliabilityLoggerTest {
    @Mock
    FeedLaunchReliabilityLogger mLaunchLogger;

    FeedReliabilityLogger mFeedReliabilityLogger;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mFeedReliabilityLogger = new FeedReliabilityLogger(mLaunchLogger);
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
        mFeedReliabilityLogger.onUrlFocusChange(/*hasFocus=*/true);
        verify(mLaunchLogger, never()).cancelPendingFinished();
    }

    @Test
    public void testOnUrlFocusChange_gainFocus_launchNotInProgress() {
        when(mLaunchLogger.isLaunchInProgress()).thenReturn(false);
        mFeedReliabilityLogger.onUrlFocusChange(/*hasFocus=*/true);
        verify(mLaunchLogger, never()).cancelPendingFinished();
    }

    @Test
    public void testOnUrlFocusChange_loseFocus_launchInProgress() {
        when(mLaunchLogger.isLaunchInProgress()).thenReturn(true);
        mFeedReliabilityLogger.onUrlFocusChange(/*hasFocus=*/false);
        verify(mLaunchLogger, times(1)).cancelPendingFinished();
    }

    @Test
    public void testOnUrlFocusChange_loseFocus_launchNotInProgress() {
        when(mLaunchLogger.isLaunchInProgress()).thenReturn(false);
        mFeedReliabilityLogger.onUrlFocusChange(/*hasFocus=*/false);
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
}