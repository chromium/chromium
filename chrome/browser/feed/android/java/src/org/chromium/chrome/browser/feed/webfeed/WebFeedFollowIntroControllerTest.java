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

import android.app.Activity;
import android.util.Base64;
import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.FeatureList;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.task.test.ShadowPostTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.feed.webfeed.WebFeedSnackbarController.FeedLauncher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.HashMap;
import java.util.concurrent.TimeUnit;

/**
 * Tests {@link WebFeedFollowIntroController}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowPostTask.class})
public final class WebFeedFollowIntroControllerTest {
    private static final long SAFE_INTRO_WAIT_TIME =
            WebFeedFollowIntroController.INTRO_WAIT_TIME_MS + 10;
    private static final GURL sTestUrl = JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL);
    private static final byte[] sWebFeedId = "webFeedId".getBytes();
    private static final SharedPreferencesManager sSharedPreferencesManager =
            SharedPreferencesManager.getInstance();

    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    FeedLauncher mFeedLauncher;
    @Mock
    private ObservableSupplier<Tab> mTabSupplier;
    @Mock
    private Tracker mTracker;
    @Mock
    private WebFeedBridge.Natives mWebFeedBridgeJniMock;
    @Mock
    private Tab mTab;
    @Mock
    private AppMenuHandler mAppMenuHandler;
    @Mock
    private SnackbarManager mSnackbarManager;
    @Mock
    private ModalDialogManager mDialogManager;
    @Mock
    private Profile mProfile;
    @Mock
    private PrefService mPrefService;
    @Mock
    private UserPrefs.Natives mUserPrefsJniMock;
    @Captor
    private ArgumentCaptor<WebFeedBridge.WebFeedPageInformation> mPageInformationCaptor;

    private int mNumVisitMin;
    private int mDailyVisitMin;
    private Activity mActivity;
    private EmptyTabObserver mEmptyTabObserver;
    private FakeClock mClock;
    private WebFeedFollowIntroController mWebFeedFollowIntroController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(WebFeedBridge.getTestHooksForTesting(), mWebFeedBridgeJniMock);
        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsJniMock);

        Profile.setLastUsedProfileForTesting(mProfile);
        Mockito.when(mUserPrefsJniMock.get(mProfile)).thenReturn(mPrefService);

        mActivity = Robolectric.setupActivity(Activity.class);
        // Required for resolving an attribute used in AppMenuItemText.
        mActivity.setTheme(R.style.Theme_BrowserUI);
        mClock = new FakeClock();
        when(mTracker.shouldTriggerHelpUI(FeatureConstants.IPH_WEB_FEED_FOLLOW_FEATURE))
                .thenReturn(true);
        when(mTab.getUrl()).thenReturn(sTestUrl);
        when(mTab.isIncognito()).thenReturn(false);
        TrackerFactory.setTrackerForTests(mTracker);

        // This empty setTestFeatures call below is needed to enable the field trial param calls.
        FeatureList.setTestFeatures(new HashMap<String, Boolean>());
        mNumVisitMin = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.WEB_FEED, PARAM_NUM_VISIT_MIN, DEFAULT_NUM_VISIT_MIN);
        mDailyVisitMin = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.WEB_FEED, PARAM_DAILY_VISIT_MIN, DEFAULT_DAILY_VISIT_MIN);

        mWebFeedFollowIntroController = new WebFeedFollowIntroController(mActivity, mAppMenuHandler,
                mTabSupplier, new View(mActivity), mFeedLauncher, mDialogManager, mSnackbarManager);
        mEmptyTabObserver = mWebFeedFollowIntroController.getEmptyTabObserverForTesting();
        mWebFeedFollowIntroController.setClockForTesting(mClock);
    }

    @After
    public void tearDown() {
        TrackerFactory.setTrackerForTests(null);
    }

    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1227610")
    public void meetsShowingRequirements_showsIntro() {
        setWebFeedIntroLastShownTimeMsPref(0);
        setWebFeedIntroWebFeedIdShownTimeMsPref(0);
        setRecommendableVisitCount(mNumVisitMin, mDailyVisitMin);
        invokePageLoad(WebFeedSubscriptionStatus.NOT_SUBSCRIBED, /*isRecommended=*/true);
        advanceClockByMs(SAFE_INTRO_WAIT_TIME);

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
    @SmallTest
    public void notRecommended_doesNotShowIntro() {
        setWebFeedIntroLastShownTimeMsPref(0);
        setWebFeedIntroWebFeedIdShownTimeMsPref(0);
        setRecommendableVisitCount(mNumVisitMin, mDailyVisitMin);
        invokePageLoad(WebFeedSubscriptionStatus.NOT_SUBSCRIBED, /*isRecommended=*/false);
        advanceClockByMs(SAFE_INTRO_WAIT_TIME);

        assertFalse("Intro should not be shown.",
                mWebFeedFollowIntroController.getIntroShownForTesting());
    }

    @Test
    @SmallTest
    public void subscribed_doesNotShowIntro() {
        setWebFeedIntroLastShownTimeMsPref(0);
        setWebFeedIntroWebFeedIdShownTimeMsPref(0);
        setRecommendableVisitCount(mNumVisitMin, mDailyVisitMin);
        invokePageLoad(WebFeedSubscriptionStatus.SUBSCRIBED, /*isRecommended=*/true);
        advanceClockByMs(SAFE_INTRO_WAIT_TIME);

        assertFalse("Intro should not be shown.",
                mWebFeedFollowIntroController.getIntroShownForTesting());
    }

    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1227610")
    public void tooShortDelay_doesNotShowIntro() {
        setWebFeedIntroLastShownTimeMsPref(0);
        setWebFeedIntroWebFeedIdShownTimeMsPref(0);
        setRecommendableVisitCount(mNumVisitMin, mDailyVisitMin);
        invokePageLoad(WebFeedSubscriptionStatus.NOT_SUBSCRIBED, /*isRecommended=*/true);
        advanceClockByMs(SAFE_INTRO_WAIT_TIME / 2);

        assertFalse("Intro should not be shown.",
                mWebFeedFollowIntroController.getIntroShownForTesting());
    }

    @Test
    @SmallTest
    public void doesNotMeetTotalVisitRequirement_doesNotShowIntro() {
        setWebFeedIntroLastShownTimeMsPref(0);
        setWebFeedIntroWebFeedIdShownTimeMsPref(0);
        setRecommendableVisitCount(mNumVisitMin - 1, mDailyVisitMin);
        invokePageLoad(WebFeedSubscriptionStatus.NOT_SUBSCRIBED, /*isRecommended=*/true);
        advanceClockByMs(SAFE_INTRO_WAIT_TIME);

        assertFalse("Intro should not be shown.",
                mWebFeedFollowIntroController.getIntroShownForTesting());
    }

    @Test
    @SmallTest
    public void doesNotMeetDailyVisitRequirement_doesNotShowIntro() {
        setWebFeedIntroLastShownTimeMsPref(0);
        setWebFeedIntroWebFeedIdShownTimeMsPref(0);
        setRecommendableVisitCount(mNumVisitMin, mDailyVisitMin - 1);
        invokePageLoad(WebFeedSubscriptionStatus.NOT_SUBSCRIBED, /*isRecommended=*/true);
        advanceClockByMs(SAFE_INTRO_WAIT_TIME);

        assertFalse("Intro should not be shown.",
                mWebFeedFollowIntroController.getIntroShownForTesting());
    }

    @Test
    @SmallTest
    public void lastShownTimeTooClose_doesNotShowIntro() {
        setWebFeedIntroLastShownTimeMsPref(mClock.currentTimeMillis() - 1000);
        setWebFeedIntroWebFeedIdShownTimeMsPref(0);
        setRecommendableVisitCount(mNumVisitMin, mDailyVisitMin);
        invokePageLoad(WebFeedSubscriptionStatus.NOT_SUBSCRIBED, /*isRecommended=*/true);
        advanceClockByMs(SAFE_INTRO_WAIT_TIME);

        assertFalse("Intro should not be shown.",
                mWebFeedFollowIntroController.getIntroShownForTesting());
    }

    @Test
    @SmallTest
    public void lastShownForWebFeedIdTimeTooClose_doesNotShowIntro() {
        setWebFeedIntroLastShownTimeMsPref(0);
        setWebFeedIntroWebFeedIdShownTimeMsPref(mClock.currentTimeMillis() - 1000);
        setRecommendableVisitCount(mNumVisitMin, mDailyVisitMin);
        invokePageLoad(WebFeedSubscriptionStatus.NOT_SUBSCRIBED, /*isRecommended=*/true);
        advanceClockByMs(SAFE_INTRO_WAIT_TIME);

        assertFalse("Intro should not be shown.",
                mWebFeedFollowIntroController.getIntroShownForTesting());
    }

    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1227610")
    public void featureEngagementTrackerSaysDoNotShow_doesNotShowIntro() {
        setWebFeedIntroLastShownTimeMsPref(0);
        setWebFeedIntroWebFeedIdShownTimeMsPref(0);
        setRecommendableVisitCount(mNumVisitMin, mDailyVisitMin);
        invokePageLoad(WebFeedSubscriptionStatus.NOT_SUBSCRIBED, /*isRecommended=*/true);
        when(mTracker.shouldTriggerHelpUI(FeatureConstants.IPH_WEB_FEED_FOLLOW_FEATURE))
                .thenReturn(false);
        advanceClockByMs(SAFE_INTRO_WAIT_TIME);

        assertFalse("Intro should not be shown.",
                mWebFeedFollowIntroController.getIntroShownForTesting());
    }

    private void setWebFeedIntroLastShownTimeMsPref(long webFeedIntroLastShownTimeMs) {
        sSharedPreferencesManager.writeLong(ChromePreferenceKeys.WEB_FEED_INTRO_LAST_SHOWN_TIME_MS,
                webFeedIntroLastShownTimeMs);
    }

    private void setWebFeedIntroWebFeedIdShownTimeMsPref(long webFeedIntroWebFeedIdShownTimeMs) {
        sSharedPreferencesManager.writeLong(
                ChromePreferenceKeys.WEB_FEED_INTRO_WEB_FEED_ID_SHOWN_TIME_MS_PREFIX.createKey(
                        Base64.encodeToString(sWebFeedId, Base64.DEFAULT)),
                webFeedIntroWebFeedIdShownTimeMs);
    }

    private void setRecommendableVisitCount(int numVisits, int dailyVisits) {
        WebFeedBridge.VisitCounts visitCounts =
                new WebFeedBridge.VisitCounts(numVisits, dailyVisits);
        doAnswer(invocation -> {
            invocation.<Callback<int[]>>getArgument(1).onResult(
                    new int[] {visitCounts.visits, visitCounts.dailyVisits});
            return null;
        })
                .when(mWebFeedBridgeJniMock)
                .getRecentVisitCountsToHost(eq(sTestUrl), any(Callback.class));
    }

    private void invokePageLoad(
            @WebFeedSubscriptionStatus int subscriptionStatus, boolean isRecommended) {
        WebFeedBridge.WebFeedMetadata webFeedMetadata =
                new WebFeedBridge.WebFeedMetadata(sWebFeedId, "title", sTestUrl, subscriptionStatus,
                        /*isActive=*/true, isRecommended);
        doAnswer(invocation -> {
            invocation.<Callback<WebFeedBridge.WebFeedMetadata>>getArgument(1).onResult(
                    webFeedMetadata);
            return null;
        })
                .when(mWebFeedBridgeJniMock)
                .findWebFeedInfoForPage(mPageInformationCaptor.capture(), any(Callback.class));

        mEmptyTabObserver.onPageLoadStarted(mTab, sTestUrl);
        mEmptyTabObserver.didFirstVisuallyNonEmptyPaint(mTab);
        mEmptyTabObserver.onPageLoadFinished(mTab, sTestUrl);
        assertEquals(sTestUrl, mPageInformationCaptor.getValue().mUrl);
    }

    private void advanceClockByMs(long timeMs) {
        mClock.advanceCurrentTimeMillis(timeMs);
        Robolectric.getForegroundThreadScheduler().advanceBy(timeMs, TimeUnit.MILLISECONDS);
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
