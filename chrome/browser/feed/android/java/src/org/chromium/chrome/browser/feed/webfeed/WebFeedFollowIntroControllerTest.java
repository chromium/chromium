// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.webfeed;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.feed.webfeed.WebFeedFollowIntroController.DEFAULT_DAILY_VISIT_MIN;
import static org.chromium.chrome.browser.feed.webfeed.WebFeedFollowIntroController.DEFAULT_NUM_VISIT_MIN;
import static org.chromium.chrome.browser.feed.webfeed.WebFeedFollowIntroController.PARAM_DAILY_VISIT_MIN;
import static org.chromium.chrome.browser.feed.webfeed.WebFeedFollowIntroController.PARAM_NUM_VISIT_MIN;

import android.util.Base64;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.feed.webfeed.WebFeedSnackbarController.FeedLauncher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.url.GURL;

/**
 * Tests {@link WebFeedFollowIntroController}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public final class WebFeedFollowIntroControllerTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Mock
    FeedLauncher mFeedLauncher;
    @Mock
    private ObservableSupplier<Tab> mTabSupplier;
    @Mock
    private Tracker mTracker;
    @Mock
    private WebFeedBridge mWebFeedBridge;
    @Mock
    private Tab mTab;

    private static final GURL sTestUrl = new GURL("https://www.example.com");
    private static final byte[] sWebFeedId = "webFeedId".getBytes();
    private static final SharedPreferencesManager sSharedPreferencesManager =
            SharedPreferencesManager.getInstance();

    private int mNumVisitMin;
    private int mDailyVisitMin;
    private AppMenuHandler mAppMenuHandler;
    private ChromeTabbedActivity mActivity;
    private EmptyTabObserver mEmptyTabObserver;
    private FakeClock mClock;
    private WebFeedFollowIntroController mWebFeedFollowIntroController;

    @Before
    public void setUp() {
        // TODO(harringtond): See if we can make this a robolectric test.
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.startMainActivityOnBlankPage();
        mActivity = mActivityTestRule.getActivity();
        mAppMenuHandler = mActivityTestRule.getAppMenuCoordinator().getAppMenuHandler();
        mClock = new FakeClock();
        when(mTracker.shouldTriggerHelpUI(FeatureConstants.IPH_WEB_FEED_FOLLOW_FEATURE))
                .thenReturn(true);
        when(mTab.getUrl()).thenReturn(sTestUrl);
        when(mTab.isIncognito()).thenReturn(false);
        TrackerFactory.setTrackerForTests(mTracker);
        mNumVisitMin = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.WEB_FEED, PARAM_NUM_VISIT_MIN, DEFAULT_NUM_VISIT_MIN);
        mDailyVisitMin = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.WEB_FEED, PARAM_DAILY_VISIT_MIN, DEFAULT_DAILY_VISIT_MIN);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mWebFeedFollowIntroController =
                    new WebFeedFollowIntroController(mActivity, mAppMenuHandler, mTabSupplier,
                            mActivity.getToolbarManager().getMenuButtonView(), mFeedLauncher,
                            mActivity.getModalDialogManager(), mActivity.getSnackbarManager(),
                            mWebFeedBridge);
        });

        mEmptyTabObserver = mWebFeedFollowIntroController.getEmptyTabObserverForTesting();
        mWebFeedFollowIntroController.setClockForTesting(mClock);
    }

    @After
    public void tearDown() {
        TrackerFactory.setTrackerForTests(null);
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void meetsShowingRequirements_showsIntro() {
        prepareForMeetingIntroRequirements();
        performScrollUpAfterDelay(WebFeedFollowIntroController.INTRO_WAIT_TIME_MS + 1);

        assertTrue(
                "Intro should be shown.", mWebFeedFollowIntroController.getIntroShownForTesting());
        assertEquals("WEB_FEED_INTRO_LAST_SHOWN_TIME_MS should be updated.",
                mClock.currentTimeMillis(),
                sSharedPreferencesManager.readLong(
                        ChromePreferenceKeys.WEB_FEED_INTRO_LAST_SHOWN_TIME_MS));
        assertEquals("WEB_FEED_INTRO_WEB_FEED_ID_SHOWN_TIME_MS_PREFIX should be updated.",
                mClock.currentTimeMillis(),
                sSharedPreferencesManager.readLong(
                        ChromePreferenceKeys.WEB_FEED_INTRO_WEB_FEED_ID_SHOWN_TIME_MS_PREFIX
                                .createKey(Base64.encodeToString(sWebFeedId, Base64.DEFAULT))));
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void notRecommended_doesNotShowIntro() {
        performScrollUpAfterDelay(WebFeedFollowIntroController.INTRO_WAIT_TIME_MS + 1);
        invokePageLoad(new WebFeedBridge.WebFeedMetadata(sWebFeedId, "title", sTestUrl,
                WebFeedSubscriptionStatus.NOT_SUBSCRIBED, /*isActive=*/
                true, /*isRecommended=*/false));

        assertFalse("Intro should not be shown.",
                mWebFeedFollowIntroController.getIntroShownForTesting());
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void subscribed_doesNotShowIntro() {
        performScrollUpAfterDelay(WebFeedFollowIntroController.INTRO_WAIT_TIME_MS + 1);
        invokePageLoad(new WebFeedBridge.WebFeedMetadata(sWebFeedId, "title", sTestUrl,
                WebFeedSubscriptionStatus.SUBSCRIBED, /*isActive=*/
                true, /*isRecommended=*/true));

        assertFalse("Intro should not be shown.",
                mWebFeedFollowIntroController.getIntroShownForTesting());
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void scrollUpWithoutDelay_doesNotShowIntro() {
        performScrollUpAfterDelay(0);

        assertFalse("Intro should not be shown.",
                mWebFeedFollowIntroController.getIntroShownForTesting());
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void doesNotMeetVisitRequirements_doesNotShowIntro() {
        setRecommendableVisitCount(
                new WebFeedBridge.VisitCounts(mNumVisitMin - 1, mDailyVisitMin - 1));
        performScrollUpAfterDelay(WebFeedFollowIntroController.INTRO_WAIT_TIME_MS + 1);

        assertFalse("Intro should not be shown.",
                mWebFeedFollowIntroController.getIntroShownForTesting());
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void lastShownTimeTooClose_doesNotShowIntro() {
        setSharedPreferences(/*webFeedIntroLastShownTimeMs=*/mClock.currentTimeMillis(),
                /*webFeedIntroWebFeedIdShownTimeMs=*/0);
        performScrollUpAfterDelay(WebFeedFollowIntroController.INTRO_WAIT_TIME_MS + 1);

        assertFalse("Intro should not be shown.",
                mWebFeedFollowIntroController.getIntroShownForTesting());
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void lastShownForWebFeedIdTimeTooClose_doesNotShowIntro() {
        setSharedPreferences(/*webFeedIntroLastShownTimeMs=*/0,
                /*webFeedIntroWebFeedIdShownTimeMs=*/mClock.currentTimeMillis());
        performScrollUpAfterDelay(WebFeedFollowIntroController.INTRO_WAIT_TIME_MS + 1);

        assertFalse("Intro should not be shown.",
                mWebFeedFollowIntroController.getIntroShownForTesting());
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void featureEngagementTrackerSaysDoNotShow_doesNotShowIntro() {
        when(mTracker.shouldTriggerHelpUI(FeatureConstants.IPH_WEB_FEED_FOLLOW_FEATURE))
                .thenReturn(false);
        performScrollUpAfterDelay(WebFeedFollowIntroController.INTRO_WAIT_TIME_MS + 1);

        assertFalse("Intro should not be shown.",
                mWebFeedFollowIntroController.getIntroShownForTesting());
    }

    private void prepareForMeetingIntroRequirements() {
        setSharedPreferences(
                /*webFeedIntroLastShownTimeMs=*/0, /*webFeedIntroWebFeedIdShownTimeMs=*/0);
        setRecommendableVisitCount(new WebFeedBridge.VisitCounts(mNumVisitMin, mDailyVisitMin));
        invokePageLoad(new WebFeedBridge.WebFeedMetadata(sWebFeedId, "title", sTestUrl,
                WebFeedSubscriptionStatus.NOT_SUBSCRIBED, /*isActive=*/
                true, /*isRecommended=*/true));
    }

    private void setSharedPreferences(
            long webFeedIntroLastShownTimeMs, long webFeedIntroWebFeedIdShownTimeMs) {
        sSharedPreferencesManager.writeLong(ChromePreferenceKeys.WEB_FEED_INTRO_LAST_SHOWN_TIME_MS,
                webFeedIntroLastShownTimeMs);
        sSharedPreferencesManager.writeLong(
                ChromePreferenceKeys.WEB_FEED_INTRO_WEB_FEED_ID_SHOWN_TIME_MS_PREFIX.createKey(
                        Base64.encodeToString(sWebFeedId, Base64.DEFAULT)),
                webFeedIntroWebFeedIdShownTimeMs);
    }

    private void setRecommendableVisitCount(WebFeedBridge.VisitCounts visitCounts) {
        doAnswer(invocation -> {
            invocation.<Callback<WebFeedBridge.VisitCounts>>getArgument(1).onResult(visitCounts);
            return null;
        })
                .when(mWebFeedBridge)
                .getVisitCountsToHost(eq(sTestUrl), any(Callback.class));
    }

    private void invokePageLoad(WebFeedBridge.WebFeedMetadata webFeedMetadata) {
        doAnswer(invocation -> {
            invocation.<Callback<WebFeedBridge.WebFeedMetadata>>getArgument(2).onResult(
                    webFeedMetadata);
            return null;
        })
                .when(mWebFeedBridge)
                .getWebFeedMetadataForPage(any(), eq(sTestUrl), any(Callback.class));

        mEmptyTabObserver.onPageLoadStarted(mTab, sTestUrl);
        mEmptyTabObserver.didFirstVisuallyNonEmptyPaint(mTab);
        mEmptyTabObserver.onPageLoadFinished(mTab, sTestUrl);
    }

    private void performScrollUpAfterDelay(long delay) {
        mClock.advanceCurrentTimeMillis(delay);
        mEmptyTabObserver.onContentViewScrollOffsetChanged(/*verticalScrollDelta=*/10);
    }

    /**
     * FakeClock for setting the time.
     */
    static class FakeClock implements WebFeedFollowIntroController.Clock {
        private long mCurrentTimeMillis;

        FakeClock() {
            mCurrentTimeMillis = 123456789;
        }

        @Override
        public long currentTimeMillis() {
            return mCurrentTimeMillis;
        }

        void advanceCurrentTimeMillis(long millis) {
            mCurrentTimeMillis += millis;
        }
    }
}
