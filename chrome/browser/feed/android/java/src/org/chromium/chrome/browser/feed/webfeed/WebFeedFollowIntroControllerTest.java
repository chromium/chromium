// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.webfeed;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.util.Base64;
import android.view.View;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowLog;

import org.chromium.base.Callback;
import org.chromium.base.FeatureList;
import org.chromium.base.UserDataHost;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.feed.webfeed.WebFeedSnackbarController.FeedLauncher;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.widget.textbubble.TextBubble;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.feature_engagement.TriggerDetails;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.concurrent.TimeUnit;

/** Tests {@link WebFeedFollowIntroController}. */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.LEGACY)
public final class WebFeedFollowIntroControllerTest {
    private static final long SAFE_INTRO_WAIT_TIME_MILLIS = 3 * 1000 + 100;
    private static final GURL sTestUrl = JUnitTestGURLs.EXAMPLE_URL;
    private static final GURL sFaviconUrl = JUnitTestGURLs.RED_1;
    private static final byte[] sWebFeedId = "webFeedId".getBytes();
    private static final SharedPreferencesManager sChromeSharedPreferences =
            ChromeSharedPreferences.getInstance();

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock FeedLauncher mFeedLauncher;
    @Mock private ObservableSupplier<Tab> mTabSupplier;
    @Mock private Tracker mTracker;
    @Mock private WebFeedBridge.Natives mWebFeedBridgeJniMock;
    @Mock private Tab mTab;
    @Mock private AppMenuHandler mAppMenuHandler;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private ModalDialogManager mDialogManager;
    @Mock private Profile mProfile;
    @Mock private PrefService mPrefService;
    @Mock private UserPrefs.Natives mUserPrefsJniMock;
    @Mock private WebContents mWebContents;
    @Mock private NavigationController mNavigationController;
    @Mock private NavigationHandle mNavigationHandle;

    private Activity mActivity;
    private EmptyTabObserver mEmptyTabObserver;
    private FakeClock mClock;
    private WebFeedFollowIntroController mWebFeedFollowIntroController;
    private FeatureList.TestValues mBaseTestValues;
    private UserDataHost mTestUserDataHost;

    @Before
    public void setUp() {
        // Print logs to stdout.
        ShadowLog.stream = System.out;

        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(WebFeedBridge.getTestHooksForTesting(), mWebFeedBridgeJniMock);
        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsJniMock);

        Mockito.when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        Mockito.when(mUserPrefsJniMock.get(mProfile)).thenReturn(mPrefService);

        // Required for resolving an attribute used in AppMenuItemText.
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        mClock = new FakeClock();
        when(mTracker.shouldTriggerHelpUI(FeatureConstants.IPH_WEB_FEED_FOLLOW_FEATURE))
                .thenReturn(true);
        when(mTracker.shouldTriggerHelpUIWithSnooze(FeatureConstants.IPH_WEB_FEED_FOLLOW_FEATURE))
                .thenReturn(new TriggerDetails(true, false));
        doAnswer(
                        invocation -> {
                            Callback<Boolean> callback = invocation.getArgument(0);
                            callback.onResult(true);
                            return null;
                        })
                .when(mTracker)
                .addOnInitializedCallback(any());

        when(mTab.getUrl()).thenReturn(sTestUrl);
        when(mTab.isIncognito()).thenReturn(false);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTabSupplier.get()).thenReturn(mTab);
        when(mWebContents.getNavigationController()).thenReturn(mNavigationController);

        mTestUserDataHost = new UserDataHost();
        WebFeedRecommendationFollowAcceleratorController.associateWebFeedWithUserData(
                mTestUserDataHost, new byte[] {1, 2, 3});

        TrackerFactory.setTrackerForTests(mTracker);

        resetWebFeedFollowIntroController();
    }

    private void resetWebFeedFollowIntroController() {
        mWebFeedFollowIntroController =
                new WebFeedFollowIntroController(
                        mActivity,
                        mProfile,
                        mAppMenuHandler,
                        mTabSupplier,
                        new View(mActivity),
                        mFeedLauncher,
                        mDialogManager,
                        mSnackbarManager);
        mEmptyTabObserver = mWebFeedFollowIntroController.getEmptyTabObserverForTesting();
        mWebFeedFollowIntroController.setClockForTesting(mClock);
        // TextBubble is impossible to show in a junit.
        TextBubble.setSkipShowCheckForTesting(true);
    }

    @After
    public void tearDown() {
        TextBubble.setSkipShowCheckForTesting(false);
    }

    @Test
    @SmallTest
    public void meetsShowingRequirements_showsIntro_Accelerator() {
        setWebFeedIntroLastShownTimeMsPref(0);
        setWebFeedIntroWebFeedIdShownTimeMsPref(0);
        setVisitCounts(3, 3);
        invokePageLoad(WebFeedSubscriptionStatus.NOT_SUBSCRIBED, /* isRecommended= */ true);
        advanceClockByMs(SAFE_INTRO_WAIT_TIME_MILLIS);

        assertTrue(
                "Intro should be shown.", mWebFeedFollowIntroController.getIntroShownForTesting());
        assertEquals(
                "WEB_FEED_INTRO_LAST_SHOWN_TIME_MS should be updated.",
                mClock.currentTimeMillis(),
                sChromeSharedPreferences.readLong(
                        ChromePreferenceKeys.WEB_FEED_INTRO_LAST_SHOWN_TIME_MS));
        assertEquals(
                "WEB_FEED_INTRO_WEB_FEED_ID_SHOWN_TIME_MS_PREFIX should be updated.",
                mClock.currentTimeMillis(),
                sChromeSharedPreferences.readLong(
                        ChromePreferenceKeys.WEB_FEED_INTRO_WEB_FEED_ID_SHOWN_TIME_MS_PREFIX
                                .createKey(Base64.encodeToString(sWebFeedId, Base64.DEFAULT))));
    }

    @Test
    @SmallTest
    public void
            meetsShowingRequirements_butPageNavigationIsFromRecommendation_acceleratorShownOnlyOnce() {
        // Mock the navigation entry to indicate the current entry contains a recommended web feed.
        when(mTab.getUserDataHost()).thenReturn(new UserDataHost());
        when(mNavigationHandle.getUserDataHost()).thenReturn(mTestUserDataHost);
        setWebFeedIntroLastShownTimeMsPref(0);
        setWebFeedIntroWebFeedIdShownTimeMsPref(0);
        setVisitCounts(3, 3);
        invokePageLoad(WebFeedSubscriptionStatus.NOT_SUBSCRIBED, /* isRecommended= */ true);
        advanceClockByMs(SAFE_INTRO_WAIT_TIME_MILLIS);

        assertFalse(mWebFeedFollowIntroController.getIntroShownForTesting());
        assertTrue(
                mWebFeedFollowIntroController
                        .getRecommendationFollowAcceleratorController()
                        .getIntroViewForTesting()
                        .wasFollowBubbleShownForTesting());
    }

    @Test
    @SmallTest
    public void meetsShowingRequirements_showsIntro_IPH() {
        resetWebFeedFollowIntroController();

        setWebFeedIntroLastShownTimeMsPref(0);
        setWebFeedIntroWebFeedIdShownTimeMsPref(0);
        setVisitCounts(3, 3);
        invokePageLoad(WebFeedSubscriptionStatus.NOT_SUBSCRIBED, /* isRecommended= */ true);
        advanceClockByMs(SAFE_INTRO_WAIT_TIME_MILLIS);

        assertTrue(
                "Intro should be shown.", mWebFeedFollowIntroController.getIntroShownForTesting());
        assertEquals(
                "WEB_FEED_INTRO_LAST_SHOWN_TIME_MS should be updated.",
                mClock.currentTimeMillis(),
                sChromeSharedPreferences.readLong(
                        ChromePreferenceKeys.WEB_FEED_INTRO_LAST_SHOWN_TIME_MS));
        assertEquals(
                "WEB_FEED_INTRO_WEB_FEED_ID_SHOWN_TIME_MS_PREFIX should be updated.",
                mClock.currentTimeMillis(),
                sChromeSharedPreferences.readLong(
                        ChromePreferenceKeys.WEB_FEED_INTRO_WEB_FEED_ID_SHOWN_TIME_MS_PREFIX
                                .createKey(Base64.encodeToString(sWebFeedId, Base64.DEFAULT))));
    }

    @Test
    @SmallTest
    public void sameWebFeedIsNotShownMoreThan3Times() {
        resetWebFeedFollowIntroController();

        mWebFeedFollowIntroController.clearIntroShownForTesting();
        setWebFeedIntroLastShownTimeMsPref(0);
        setWebFeedIntroWebFeedIdShownTimeMsPref(0);
        setVisitCounts(3, 3);
        invokePageLoad(WebFeedSubscriptionStatus.NOT_SUBSCRIBED, /* isRecommended= */ true);
        advanceClockByMs(SAFE_INTRO_WAIT_TIME_MILLIS);
        assertTrue(
                "Intro should be shown first time",
                mWebFeedFollowIntroController.getIntroShownForTesting());

        mWebFeedFollowIntroController.clearIntroShownForTesting();
        setWebFeedIntroLastShownTimeMsPref(0);
        setWebFeedIntroWebFeedIdShownTimeMsPref(0);
        setVisitCounts(3, 3);
        invokePageLoad(WebFeedSubscriptionStatus.NOT_SUBSCRIBED, /* isRecommended= */ true);
        advanceClockByMs(SAFE_INTRO_WAIT_TIME_MILLIS);
        assertTrue(
                "Intro should be shown second time",
                mWebFeedFollowIntroController.getIntroShownForTesting());

        mWebFeedFollowIntroController.clearIntroShownForTesting();
        setWebFeedIntroLastShownTimeMsPref(0);
        setWebFeedIntroWebFeedIdShownTimeMsPref(0);
        setVisitCounts(3, 3);
        invokePageLoad(WebFeedSubscriptionStatus.NOT_SUBSCRIBED, /* isRecommended= */ true);
        advanceClockByMs(SAFE_INTRO_WAIT_TIME_MILLIS);
        assertTrue(
                "Intro should be shown third time",
                mWebFeedFollowIntroController.getIntroShownForTesting());

        mWebFeedFollowIntroController.clearIntroShownForTesting();
        setWebFeedIntroLastShownTimeMsPref(0);
        setWebFeedIntroWebFeedIdShownTimeMsPref(0);
        setVisitCounts(3, 3);
        invokePageLoad(WebFeedSubscriptionStatus.NOT_SUBSCRIBED, /* isRecommended= */ true);
        advanceClockByMs(SAFE_INTRO_WAIT_TIME_MILLIS);
        assertFalse(
                "Intro should NOT be shown fourth time",
                mWebFeedFollowIntroController.getIntroShownForTesting());
    }

    @Test
    @SmallTest
    public void secondPageLoad_cancelsPreviousIntro() {
        setWebFeedIntroLastShownTimeMsPref(0);
        setWebFeedIntroWebFeedIdShownTimeMsPref(0);
        setVisitCounts(3, 3);

        // This page load would trigger the intro, but a second page load is fired before that can
        // happen.
        invokePageLoad(WebFeedSubscriptionStatus.NOT_SUBSCRIBED, /* isRecommended= */ true);
        advanceClockByMs(SAFE_INTRO_WAIT_TIME_MILLIS / 2);

        invokePageLoad(WebFeedSubscriptionStatus.NOT_SUBSCRIBED, /* isRecommended= */ true);
        advanceClockByMs(SAFE_INTRO_WAIT_TIME_MILLIS - 500);
        assertFalse(
                "Intro should not be shown yet.",
                mWebFeedFollowIntroController.getIntroShownForTesting());

        advanceClockByMs(500);

        assertTrue(
                "Intro should be shown.", mWebFeedFollowIntroController.getIntroShownForTesting());
    }

    @Test
    @SmallTest
    public void notRecommended_doesNotShowIntro() {
        setWebFeedIntroLastShownTimeMsPref(0);
        setWebFeedIntroWebFeedIdShownTimeMsPref(0);
        setVisitCounts(3, 3);
        invokePageLoad(WebFeedSubscriptionStatus.NOT_SUBSCRIBED, /* isRecommended= */ false);
        advanceClockByMs(SAFE_INTRO_WAIT_TIME_MILLIS);

        assertFalse(
                "Intro should not be shown.",
                mWebFeedFollowIntroController.getIntroShownForTesting());
    }

    @Test
    @SmallTest
    public void subscribed_doesNotShowIntro() {
        setWebFeedIntroLastShownTimeMsPref(0);
        setWebFeedIntroWebFeedIdShownTimeMsPref(0);
        setVisitCounts(3, 3);
        invokePageLoad(WebFeedSubscriptionStatus.SUBSCRIBED, /* isRecommended= */ true);
        advanceClockByMs(SAFE_INTRO_WAIT_TIME_MILLIS);

        assertFalse(
                "Intro should not be shown.",
                mWebFeedFollowIntroController.getIntroShownForTesting());
    }

    @Test
    @SmallTest
    public void tooShortDelay_doesNotShowIntro() {
        setWebFeedIntroLastShownTimeMsPref(0);
        setWebFeedIntroWebFeedIdShownTimeMsPref(0);
        setVisitCounts(3, 3);
        invokePageLoad(WebFeedSubscriptionStatus.NOT_SUBSCRIBED, /* isRecommended= */ true);
        advanceClockByMs(SAFE_INTRO_WAIT_TIME_MILLIS / 2);

        assertFalse(
                "Intro should not be shown.",
                mWebFeedFollowIntroController.getIntroShownForTesting());
    }

    @Test
    @SmallTest
    public void doesNotMeetTotalVisitRequirement_doesNotShowIntro() {
        setWebFeedIntroLastShownTimeMsPref(0);
        setWebFeedIntroWebFeedIdShownTimeMsPref(0);
        setVisitCounts(2, 3);
        invokePageLoad(WebFeedSubscriptionStatus.NOT_SUBSCRIBED, /* isRecommended= */ true);
        advanceClockByMs(SAFE_INTRO_WAIT_TIME_MILLIS);

        assertFalse(
                "Intro should not be shown.",
                mWebFeedFollowIntroController.getIntroShownForTesting());
    }

    @Test
    @SmallTest
    public void doesNotMeetDailyVisitRequirement_doesNotShowIntro() {
        setWebFeedIntroLastShownTimeMsPref(0);
        setWebFeedIntroWebFeedIdShownTimeMsPref(0);
        setVisitCounts(3, 2);
        invokePageLoad(WebFeedSubscriptionStatus.NOT_SUBSCRIBED, /* isRecommended= */ true);
        advanceClockByMs(SAFE_INTRO_WAIT_TIME_MILLIS);

        assertFalse(
                "Intro should not be shown.",
                mWebFeedFollowIntroController.getIntroShownForTesting());
    }

    @Test
    @SmallTest
    public void lastShownTimeTooClose_doesNotShowIntro() {
        setWebFeedIntroLastShownTimeMsPref(mClock.currentTimeMillis() - 1000);
        setWebFeedIntroWebFeedIdShownTimeMsPref(0);
        setVisitCounts(3, 3);
        invokePageLoad(WebFeedSubscriptionStatus.NOT_SUBSCRIBED, /* isRecommended= */ true);
        advanceClockByMs(SAFE_INTRO_WAIT_TIME_MILLIS);

        assertFalse(
                "Intro should not be shown.",
                mWebFeedFollowIntroController.getIntroShownForTesting());
    }

    @Test
    @SmallTest
    public void lastShownForWebFeedIdTimeTooClose_doesNotShowIntro() {
        setWebFeedIntroLastShownTimeMsPref(0);
        setWebFeedIntroWebFeedIdShownTimeMsPref(mClock.currentTimeMillis() - 1000);
        setVisitCounts(3, 3);
        invokePageLoad(WebFeedSubscriptionStatus.NOT_SUBSCRIBED, /* isRecommended= */ true);
        advanceClockByMs(SAFE_INTRO_WAIT_TIME_MILLIS);

        assertFalse(
                "Intro should not be shown.",
                mWebFeedFollowIntroController.getIntroShownForTesting());
    }

    @Test
    @SmallTest
    public void featureEngagementTrackerSaysDoNotShow_doesNotShowIntro() {
        setWebFeedIntroLastShownTimeMsPref(0);
        setWebFeedIntroWebFeedIdShownTimeMsPref(0);
        setVisitCounts(3, 3);
        invokePageLoad(WebFeedSubscriptionStatus.NOT_SUBSCRIBED, /* isRecommended= */ true);
        when(mTracker.shouldTriggerHelpUI(FeatureConstants.IPH_WEB_FEED_FOLLOW_FEATURE))
                .thenReturn(false);
        when(mTracker.shouldTriggerHelpUIWithSnooze(FeatureConstants.IPH_WEB_FEED_FOLLOW_FEATURE))
                .thenReturn(new TriggerDetails(false, false));
        advanceClockByMs(SAFE_INTRO_WAIT_TIME_MILLIS);

        assertFalse(
                "Intro should not be shown.",
                mWebFeedFollowIntroController.getIntroShownForTesting());
    }

    private void setWebFeedIntroLastShownTimeMsPref(long webFeedIntroLastShownTimeMs) {
        sChromeSharedPreferences.writeLong(
                ChromePreferenceKeys.WEB_FEED_INTRO_LAST_SHOWN_TIME_MS,
                webFeedIntroLastShownTimeMs);
    }

    private void setWebFeedIntroWebFeedIdShownTimeMsPref(long webFeedIntroWebFeedIdShownTimeMs) {
        sChromeSharedPreferences.writeLong(
                ChromePreferenceKeys.WEB_FEED_INTRO_WEB_FEED_ID_SHOWN_TIME_MS_PREFIX.createKey(
                        Base64.encodeToString(sWebFeedId, Base64.DEFAULT)),
                webFeedIntroWebFeedIdShownTimeMs);
    }

    private void setVisitCounts(int totalVisits, int dailyVisits) {
        WebFeedBridge.VisitCounts visitCounts =
                new WebFeedBridge.VisitCounts(totalVisits, dailyVisits);
        doAnswer(
                        invocation -> {
                            invocation
                                    .<Callback<int[]>>getArgument(1)
                                    .onResult(
                                            new int[] {
                                                visitCounts.visits, visitCounts.dailyVisits
                                            });
                            return null;
                        })
                .when(mWebFeedBridgeJniMock)
                .getRecentVisitCountsToHost(eq(sTestUrl), any(Callback.class));
    }

    private void invokePageLoad(
            @WebFeedSubscriptionStatus int subscriptionStatus, boolean isRecommended) {
        // We don't check for web feed name unless the navigation is on the main frame.
        when(mNavigationHandle.isInPrimaryMainFrame()).thenReturn(true);
        // Set the navigation handle so we can get (mocked) user data out of it.
        mEmptyTabObserver.onDidFinishNavigationInPrimaryMainFrame(mTab, mNavigationHandle);

        WebFeedBridge.WebFeedMetadata webFeedMetadata =
                new WebFeedBridge.WebFeedMetadata(
                        sWebFeedId,
                        "title",
                        sTestUrl,
                        subscriptionStatus,
                        WebFeedAvailabilityStatus.ACTIVE,
                        isRecommended,
                        sFaviconUrl);

        // Respond to the findWebFeedInfoForPage JNI api by calling the callback.
        doAnswer(
                        invocation -> {
                            assertEquals(
                                    "Incorrect WebFeedPageInformationRequestReason was used.",
                                    WebFeedPageInformationRequestReason.FOLLOW_RECOMMENDATION,
                                    invocation.<Integer>getArgument(1).intValue());
                            invocation
                                    .<Callback<WebFeedBridge.WebFeedMetadata>>getArgument(2)
                                    .onResult(webFeedMetadata);
                            return null;
                        })
                .when(mWebFeedBridgeJniMock)
                .findWebFeedInfoForPage(
                        any(WebFeedBridge.WebFeedPageInformation.class),
                        anyInt(),
                        any(Callback.class));

        // Respond to the findWebFeedInfoForWebFeedId JNI api by calling the callback.
        doAnswer(
                        invocation -> {
                            invocation
                                    .<Callback<WebFeedBridge.WebFeedMetadata>>getArgument(1)
                                    .onResult(webFeedMetadata);
                            return null;
                        })
                .when(mWebFeedBridgeJniMock)
                .findWebFeedInfoForWebFeedId(any(), any(Callback.class));

        mEmptyTabObserver.onPageLoadStarted(mTab, sTestUrl);
        mEmptyTabObserver.didFirstVisuallyNonEmptyPaint(mTab);
        mEmptyTabObserver.onPageLoadFinished(mTab, sTestUrl);
    }

    private void advanceClockByMs(long timeMs) {
        mClock.advanceCurrentTimeMillis(timeMs);
        Robolectric.getForegroundThreadScheduler().advanceBy(timeMs, TimeUnit.MILLISECONDS);
    }

    /** FakeClock for setting the time. */
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
