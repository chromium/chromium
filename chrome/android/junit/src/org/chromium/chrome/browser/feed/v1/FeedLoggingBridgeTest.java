// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.feed.v1;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.support.test.filters.SmallTest;

import androidx.annotation.Nullable;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.feed.library.api.host.logging.ActionType;
import org.chromium.chrome.browser.feed.library.api.host.logging.ContentLoggingData;
import org.chromium.chrome.browser.feed.library.api.host.logging.ScrollType;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;

/** Tests of the {@link FeedLoggingBridge} class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowRecordHistogram.class})
@Features.DisableFeatures({ChromeFeatureList.INTEREST_FEEDV1_CLICKS_AND_VIEWS_CONDITIONAL_UPLOAD,
        ChromeFeatureList.INTEREST_FEED_NOTICE_CARD_AUTO_DISMISS})
public class FeedLoggingBridgeTest {
    private static final String HISTOGRAM_ENGAGEMENT_TYPE =
            "ContentSuggestions.Feed.EngagementType";
    private static final long LESS_THAN_FIVE_MINUTES_IN_MILLISECONDS = (1000 * 60 * 5) - 1;
    private static final long MORE_THAN_FIVE_MINUTES_IN_MILLISECONDS = (1000 * 60 * 5) + 1;
    private static final int SHORT_SCROLL_DISTANCE = 10;
    private static final int LONG_SCROLL_DISTANCE = 1000;
    private FeedLoggingBridge mFeedLoggingBridge;
    private FakeClock mFakeClock;

    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();

    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    private FeedLoggingBridge.Natives mFeedLoggingBridgeJniMock;

    @Mock
    private UserPrefs.Natives mUserPrefsJniMock;

    @Mock
    private Profile mProfile;

    @Mock
    private PrefService mPrefService;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        ShadowRecordHistogram.reset();
        mocker.mock(FeedLoggingBridgeJni.TEST_HOOKS, mFeedLoggingBridgeJniMock);
        when(mFeedLoggingBridgeJniMock.init(any(), any())).thenReturn((long) 1);

        mocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsJniMock);
        Profile.setLastUsedProfileForTesting(mProfile);
        when(mUserPrefsJniMock.get(mProfile)).thenReturn(mPrefService);

        when(mPrefService.getBoolean(Pref.LAST_REFRESH_WAS_SIGNED_IN)).thenReturn(true);

        Profile profile = null;
        mFakeClock = new FakeClock();
        mFeedLoggingBridge = new FeedLoggingBridge(profile, mFakeClock);
    }

    @After
    public void tearDown() {}

    @Test
    @SmallTest
    @Feature({"Feed"})
    public void reportScrollActivity() throws Exception {
        verifyHistogram(FeedLoggingBridge.FeedEngagementType.FEED_SCROLLED, 0);
        verifyHistogram(FeedLoggingBridge.FeedEngagementType.FEED_ENGAGED, 0);
        verifyHistogram(FeedLoggingBridge.FeedEngagementType.FEED_ENGAGED_SIMPLE, 0);

        mFeedLoggingBridge.onScroll(ScrollType.STREAM_SCROLL, LONG_SCROLL_DISTANCE);

        verifyHistogram(FeedLoggingBridge.FeedEngagementType.FEED_SCROLLED, 1);
        verifyHistogram(FeedLoggingBridge.FeedEngagementType.FEED_ENGAGED, 1);
        verifyHistogram(FeedLoggingBridge.FeedEngagementType.FEED_ENGAGED_SIMPLE, 1);
    }

    @Test
    @SmallTest
    @Feature({"Feed"})
    public void reportFeedInteraction() throws Exception {
        verifyHistogram(FeedLoggingBridge.FeedEngagementType.FEED_INTERACTED, 0);
        verifyHistogram(FeedLoggingBridge.FeedEngagementType.FEED_ENGAGED, 0);
        verifyHistogram(FeedLoggingBridge.FeedEngagementType.FEED_ENGAGED_SIMPLE, 0);

        mFeedLoggingBridge.reportFeedInteraction();

        verifyHistogram(FeedLoggingBridge.FeedEngagementType.FEED_INTERACTED, 1);
        verifyHistogram(FeedLoggingBridge.FeedEngagementType.FEED_ENGAGED, 1);
        verifyHistogram(FeedLoggingBridge.FeedEngagementType.FEED_ENGAGED_SIMPLE, 1);
    }

    @Test
    @SmallTest
    @Feature({"Feed"})
    public void smallScroll() throws Exception {
        verifyHistogram(FeedLoggingBridge.FeedEngagementType.FEED_ENGAGED, 0);
        verifyHistogram(FeedLoggingBridge.FeedEngagementType.FEED_ENGAGED_SIMPLE, 0);

        // A small scroll should count as simple, but not engaged.
        mFeedLoggingBridge.onScroll(ScrollType.STREAM_SCROLL, SHORT_SCROLL_DISTANCE);

        verifyHistogram(FeedLoggingBridge.FeedEngagementType.FEED_ENGAGED, 0);
        verifyHistogram(FeedLoggingBridge.FeedEngagementType.FEED_ENGAGED_SIMPLE, 1);
        verifyHistogram(FeedLoggingBridge.FeedEngagementType.FEED_SCROLLED, 1);
    }

    @Test
    @SmallTest
    @Feature({"Feed"})
    public void negativeScroll() throws Exception {
        mFeedLoggingBridge.onScroll(ScrollType.STREAM_SCROLL, -1);

        verifyHistogram(FeedLoggingBridge.FeedEngagementType.FEED_ENGAGED_SIMPLE, 1);
        verifyHistogram(FeedLoggingBridge.FeedEngagementType.FEED_SCROLLED, 1);
    }

    @Test
    @SmallTest
    @Feature({"Feed"})
    public void multipleCalls() throws Exception {
        verifyHistogram(FeedLoggingBridge.FeedEngagementType.FEED_ENGAGED, 0);
        verifyHistogram(FeedLoggingBridge.FeedEngagementType.FEED_ENGAGED_SIMPLE, 0);

        // If we interact several times, we only report engaged once.
        mFeedLoggingBridge.reportFeedInteraction();
        mFeedLoggingBridge.reportFeedInteraction();
        mFeedLoggingBridge.onScroll(ScrollType.STREAM_SCROLL, LONG_SCROLL_DISTANCE);

        verifyHistogram(FeedLoggingBridge.FeedEngagementType.FEED_ENGAGED, 1);
        verifyHistogram(FeedLoggingBridge.FeedEngagementType.FEED_ENGAGED_SIMPLE, 1);
    }

    @Test
    @SmallTest
    @Feature({"Feed"})
    public void oncePerDay() throws Exception {
        verifyHistogram(FeedLoggingBridge.FeedEngagementType.FEED_ENGAGED, 0);
        verifyHistogram(FeedLoggingBridge.FeedEngagementType.FEED_ENGAGED_SIMPLE, 0);

        mFeedLoggingBridge.reportFeedInteraction();
        mFeedLoggingBridge.onScroll(ScrollType.STREAM_SCROLL, LONG_SCROLL_DISTANCE);

        verifyHistogram(FeedLoggingBridge.FeedEngagementType.FEED_ENGAGED, 1);
        verifyHistogram(FeedLoggingBridge.FeedEngagementType.FEED_ENGAGED_SIMPLE, 1);
        verifyHistogram(FeedLoggingBridge.FeedEngagementType.FEED_SCROLLED, 1);

        // Make almost 5 minutes pass then an interaction, it should not be counted as a new visit.
        mFakeClock.advance(LESS_THAN_FIVE_MINUTES_IN_MILLISECONDS);
        mFeedLoggingBridge.reportFeedInteraction();
        mFeedLoggingBridge.onScroll(ScrollType.STREAM_SCROLL, LONG_SCROLL_DISTANCE);

        verifyHistogram(FeedLoggingBridge.FeedEngagementType.FEED_ENGAGED, 1);
        verifyHistogram(FeedLoggingBridge.FeedEngagementType.FEED_ENGAGED_SIMPLE, 1);
        verifyHistogram(FeedLoggingBridge.FeedEngagementType.FEED_SCROLLED, 1);

        // Make more than 5 minutes pass with no interaction, should be counted as a new visit.
        mFakeClock.advance(MORE_THAN_FIVE_MINUTES_IN_MILLISECONDS);
        mFeedLoggingBridge.reportFeedInteraction();
        mFeedLoggingBridge.onScroll(ScrollType.STREAM_SCROLL, LONG_SCROLL_DISTANCE);

        verifyHistogram(FeedLoggingBridge.FeedEngagementType.FEED_ENGAGED, 2);
        verifyHistogram(FeedLoggingBridge.FeedEngagementType.FEED_ENGAGED_SIMPLE, 2);
        verifyHistogram(FeedLoggingBridge.FeedEngagementType.FEED_SCROLLED, 2);
    }

    @Test
    @SmallTest
    @Feature({"Feed"})
    @Features.EnableFeatures(ChromeFeatureList.INTEREST_FEEDV1_CLICKS_AND_VIEWS_CONDITIONAL_UPLOAD)
    public void onContentViewed_setPrefOnce_whenReachLoggingThresholdAndFeatureEnabled()
            throws Exception {
        ContentLoggingData data = makeContentData(2);
        mFeedLoggingBridge.onContentViewed(data);
        mFeedLoggingBridge.onContentViewed(data);

        verify(mPrefService, times(1))
                .setBoolean(Pref.HAS_REACHED_CLICK_AND_VIEW_ACTIONS_UPLOAD_CONDITIONS, true);
    }

    @Test
    @SmallTest
    @Feature({"Feed"})
    @Features.DisableFeatures(ChromeFeatureList.INTEREST_FEEDV1_CLICKS_AND_VIEWS_CONDITIONAL_UPLOAD)
    public void onContentViewed_dontSetPref_whenReachLoggingThresholdAndFeatureDisabled()
            throws Exception {
        ContentLoggingData data = makeContentData(2);
        mFeedLoggingBridge.onContentViewed(data);

        verify(mPrefService, never())
                .setBoolean(Pref.HAS_REACHED_CLICK_AND_VIEW_ACTIONS_UPLOAD_CONDITIONS, true);
    }

    @Test
    @SmallTest
    @Feature({"Feed"})
    @Features.EnableFeatures(ChromeFeatureList.INTEREST_FEEDV1_CLICKS_AND_VIEWS_CONDITIONAL_UPLOAD)
    public void onContentViewed_dontSetPref_whenLastRefreshWasNotSignedIn() throws Exception {
        when(mPrefService.getBoolean(Pref.LAST_REFRESH_WAS_SIGNED_IN)).thenReturn(false);

        ContentLoggingData data = makeContentData(2);
        mFeedLoggingBridge.onContentViewed(data);

        verify(mPrefService, never())
                .setBoolean(Pref.HAS_REACHED_CLICK_AND_VIEW_ACTIONS_UPLOAD_CONDITIONS, true);
    }

    @Test
    @SmallTest
    @Feature({"Feed"})
    @Features.EnableFeatures({ChromeFeatureList.INTEREST_FEED_NOTICE_CARD_AUTO_DISMISS,
            ChromeFeatureList.INTEREST_FEEDV1_CLICKS_AND_VIEWS_CONDITIONAL_UPLOAD})
    public void
    onContentViewed_updateClicksAndViewsCount_whenNoticeCardAt2ndPos() throws Exception {
        when(mPrefService.getBoolean(Pref.LAST_FETCH_HAD_NOTICE_CARD)).thenReturn(true);

        int initialCount = 1;
        when(mPrefService.getInteger(Pref.NOTICE_CARD_CLICKS_COUNT)).thenReturn(initialCount);
        when(mPrefService.getInteger(Pref.NOTICE_CARD_VIEWS_COUNT)).thenReturn(initialCount);

        // Determine the notice card index to be one because the conditional logging feature is
        // enabled which is tied to having the notice card at the 2nd position.
        int noticeCardIndex = 1;
        ContentLoggingData data = makeContentData(noticeCardIndex);
        mFeedLoggingBridge.onContentViewed(data);
        mFeedLoggingBridge.onClientAction(data, ActionType.UNKNOWN);

        // Verify that the counts are incremented by one.
        verify(mPrefService, times(1)).setInteger(Pref.NOTICE_CARD_CLICKS_COUNT, initialCount + 1);
        verify(mPrefService, times(1)).setInteger(Pref.NOTICE_CARD_VIEWS_COUNT, initialCount + 1);
    }

    @Test
    @SmallTest
    @Feature({"Feed"})
    @Features.EnableFeatures({ChromeFeatureList.INTEREST_FEED_NOTICE_CARD_AUTO_DISMISS})
    public void onContentViewed_updateClicksAndViewsCount_whenNoticeCardAt1stPos()
            throws Exception {
        when(mPrefService.getBoolean(Pref.LAST_FETCH_HAD_NOTICE_CARD)).thenReturn(true);

        int initialCount = 1;
        when(mPrefService.getInteger(Pref.NOTICE_CARD_CLICKS_COUNT)).thenReturn(initialCount);
        when(mPrefService.getInteger(Pref.NOTICE_CARD_VIEWS_COUNT)).thenReturn(initialCount);

        // Determine the notice card index to be zero because the notice card at the 1st position
        // when conditional logging is disabled.
        int noticeCardIndex = 0;
        ContentLoggingData data = makeContentData(noticeCardIndex);
        mFeedLoggingBridge.onContentViewed(data);
        mFeedLoggingBridge.onClientAction(data, ActionType.UNKNOWN);

        // Verify that the counts are incremented by one.
        verify(mPrefService, times(1)).setInteger(Pref.NOTICE_CARD_CLICKS_COUNT, initialCount + 1);
        verify(mPrefService, times(1)).setInteger(Pref.NOTICE_CARD_VIEWS_COUNT, initialCount + 1);
    }

    @Test
    @SmallTest
    @Feature({"Feed"})
    public void onContentViewed_dontUpdateClicksAndViewsCount_whenFeatureDisabled()
            throws Exception {
        when(mPrefService.getBoolean(Pref.LAST_FETCH_HAD_NOTICE_CARD)).thenReturn(true);

        // Determine the notice card index to be zero because the notice card is at the 1st position
        // when conditional logging is disabled.
        int noticeCardIndex = 0;
        ContentLoggingData data = makeContentData(noticeCardIndex);
        mFeedLoggingBridge.onContentViewed(data);
        mFeedLoggingBridge.onClientAction(data, ActionType.UNKNOWN);

        // Verify that the counts are not incremented.
        verify(mPrefService, never()).setInteger(eq(Pref.NOTICE_CARD_CLICKS_COUNT), anyInt());
        verify(mPrefService, never()).setInteger(eq(Pref.NOTICE_CARD_VIEWS_COUNT), anyInt());
    }

    @Test
    @SmallTest
    @Feature({"Feed"})
    @Features.EnableFeatures({ChromeFeatureList.INTEREST_FEED_NOTICE_CARD_AUTO_DISMISS})
    public void onContentViewed_dontUpdateClicksAndViewsCount_whenNoNoticeCard() throws Exception {
        when(mPrefService.getBoolean(Pref.LAST_FETCH_HAD_NOTICE_CARD)).thenReturn(false);

        // Determine the notice card index to be zero because the notice card at the 1st position
        // when conditional logging is disabled.
        int noticeCardIndex = 0;
        ContentLoggingData data = makeContentData(noticeCardIndex);
        mFeedLoggingBridge.onContentViewed(data);
        mFeedLoggingBridge.onClientAction(data, ActionType.UNKNOWN);

        // Verify that the counts are not incremented.
        verify(mPrefService, never()).setInteger(eq(Pref.NOTICE_CARD_CLICKS_COUNT), anyInt());
        verify(mPrefService, never()).setInteger(eq(Pref.NOTICE_CARD_VIEWS_COUNT), anyInt());
    }

    @Test
    @SmallTest
    @Feature({"Feed"})
    @Features.EnableFeatures({ChromeFeatureList.INTEREST_FEED_NOTICE_CARD_AUTO_DISMISS})
    public void onContentViewed_dontUpdateClicksAndViewsCount_whenNotNoticeCardIndex()
            throws Exception {
        when(mPrefService.getBoolean(Pref.LAST_FETCH_HAD_NOTICE_CARD)).thenReturn(true);

        int nonNoticeCardIndex = 4;
        ContentLoggingData data = makeContentData(nonNoticeCardIndex);
        mFeedLoggingBridge.onContentViewed(data);
        mFeedLoggingBridge.onClientAction(data, ActionType.UNKNOWN);

        // Verify that the counts are not incremented.
        verify(mPrefService, never()).setInteger(eq(Pref.NOTICE_CARD_CLICKS_COUNT), anyInt());
        verify(mPrefService, never()).setInteger(eq(Pref.NOTICE_CARD_VIEWS_COUNT), anyInt());
    }

    private void verifyHistogram(int sample, int expectedCount) {
        assertEquals(expectedCount,
                RecordHistogram.getHistogramValueCountForTesting(
                        HISTOGRAM_ENGAGEMENT_TYPE, sample));
    }

    private ContentLoggingData makeContentData(int positionInStream) {
        return new ContentLoggingData() {
            @Override
            public int getPositionInStream() {
                return positionInStream;
            }

            @Override
            public long getPublishedTimeSeconds() {
                return 0;
            }

            @Override
            public long getTimeContentBecameAvailable() {
                return 0;
            }

            @Override
            public float getScore() {
                return 0.0f;
            }

            @Override
            public String getRepresentationUri() {
                return "";
            }

            @Override
            public boolean isAvailableOffline() {
                return true;
            }

            @Override
            public int hashCode() {
                return 0;
            }

            @Override
            public boolean equals(@Nullable Object o) {
                return true;
            }

            @Override
            public String toString() {
                return "";
            }
        };
    }
}
