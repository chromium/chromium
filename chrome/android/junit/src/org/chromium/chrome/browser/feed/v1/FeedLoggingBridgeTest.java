// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.feed.v1;

import static org.junit.Assert.assertEquals;

import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.feed.library.api.host.logging.ScrollType;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.chrome.browser.profiles.Profile;

/** Tests of the {@link FeedLoggingBridge} class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowRecordHistogram.class})
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
    public JniMocker mocker = new JniMocker();

    @Mock
    private FeedLoggingBridge.Natives mFeedLoggingBridgeJniMock;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        ShadowRecordHistogram.reset();
        mocker.mock(FeedLoggingBridgeJni.TEST_HOOKS, mFeedLoggingBridgeJniMock);
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

    private void verifyHistogram(int sample, int expectedCount) {
        assertEquals(expectedCount,
                RecordHistogram.getHistogramValueCountForTesting(
                        HISTOGRAM_ENGAGEMENT_TYPE, sample));
    }
}
