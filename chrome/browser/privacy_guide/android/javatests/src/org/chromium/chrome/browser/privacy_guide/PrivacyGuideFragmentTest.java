// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.intent.Intents.intended;
import static androidx.test.espresso.intent.Intents.intending;
import static androidx.test.espresso.intent.matcher.IntentMatchers.anyIntent;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.Intent;

import androidx.test.espresso.intent.Intents;
import androidx.test.espresso.intent.matcher.IntentMatchers;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.metrics.HistogramTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridge;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.content_settings.CookieControlsMode;
import org.chromium.components.content_settings.PrefNames;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;

import java.util.HashSet;
import java.util.Set;

/**
 * Tests {@link PrivacyGuideFragment}
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PrivacyGuideFragmentTest {
    private static final String SETTINGS_STATES_HISTOGRAM = "Settings.PrivacyGuide.SettingsStates";
    private static final String NEXT_NAVIGATION_HISTOGRAM = "Settings.PrivacyGuide.NextNavigation";
    private static final String ENTRY_EXIT_HISTOGRAM = "Settings.PrivacyGuide.EntryExit";

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public HistogramTestRule mHistogramTestRule = new HistogramTestRule();

    @Rule
    public SettingsActivityTestRule<PrivacyGuideFragment> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(PrivacyGuideFragment.class);

    @Rule
    public SigninTestRule mSigninTestRule = new SigninTestRule();

    @Mock
    private PrivacyGuideMetricsDelegate mPrivacyGuideMetricsDelegateMock;

    private UserActionTester mActionTester;

    @BeforeClass
    public static void setUpBeforeActivityLaunched() {
        // Only needs to be loaded once and needs to be loaded before HistogramTestRule.
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
    }

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
        mSigninTestRule.addTestAccountThenSigninAndEnableSync();
        mActionTester = new UserActionTester();
    }

    @After
    public void tearDown() {
        mActionTester.tearDown();
        mActionTester = null;
    }

    private void launchPrivacyGuide() {
        mSettingsActivityTestRule.startSettingsActivity();
        onViewWaiting(withText(R.string.prefs_privacy_guide_title));
    }

    private void navigateFromWelcomeToMSBBCard() {
        onView(withText(R.string.privacy_guide_start_button)).perform(click());
        onViewWaiting(withText(R.string.url_keyed_anonymized_data_title));
    }

    private void navigateFromMSBBToHistorySyncCard() {
        onView(withText(R.string.next)).perform(click());
        onViewWaiting(withText(R.string.privacy_guide_history_sync_toggle));
    }

    private void navigateFromHistorySyncToSBCard() {
        onView(withText(R.string.next)).perform(click());
        onViewWaiting(withText(R.string.privacy_guide_safe_browsing_intro));
    }

    private void navigateFromHistorySyncToMSBBCard() {
        onView(withText(R.string.back)).perform(click());
        onViewWaiting(withText(R.string.url_keyed_anonymized_data_title));
    }

    private void navigateFromSBToCookiesCard() {
        onView(withText(R.string.next)).perform(click());
        onViewWaiting(withText(R.string.privacy_guide_cookies_intro));
    }

    private void navigateFromSBToHistorySyncCard() {
        onView(withText(R.string.back)).perform(click());
        onViewWaiting(withText(R.string.privacy_guide_history_sync_toggle));
    }

    private void navigateFromCookiesToCompletionCard() {
        onView(withText(R.string.privacy_guide_finish_button)).perform(click());
        onViewWaiting(withText(R.string.privacy_guide_done_title));
    }

    private void navigateFromCookiesToSBCard() {
        onView(withText(R.string.back)).perform(click());
        onViewWaiting(withText(R.string.privacy_guide_safe_browsing_intro));
    }

    private void goToHistorySyncCard() {
        navigateFromWelcomeToMSBBCard();
        navigateFromMSBBToHistorySyncCard();
    }

    private void goToSafeBrowsingCard() {
        goToHistorySyncCard();
        navigateFromHistorySyncToSBCard();
    }

    private void goToCookiesCard() {
        goToSafeBrowsingCard();
        navigateFromSBToCookiesCard();
    }

    private void goToCompletionCard() {
        goToCookiesCard();
        navigateFromCookiesToCompletionCard();
    }

    private void setMSBBState(boolean isMSBBon) {
        runOnUiThreadBlocking(
                ()
                        -> UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(
                                Profile.getLastUsedRegularProfile(), isMSBBon));
    }

    private void setHistorySyncState(boolean isHistorySyncOn) {
        Set<Integer> selectedTypes = new HashSet<>();
        if (isHistorySyncOn) {
            selectedTypes.add(UserSelectableType.HISTORY);
        } else {
            selectedTypes.remove(UserSelectableType.HISTORY);
        }

        runOnUiThreadBlocking(() -> SyncService.get().setSelectedTypes(false, selectedTypes));
    }

    private void setSafeBrowsingState(@SafeBrowsingState int safeBrowsingState) {
        runOnUiThreadBlocking(() -> SafeBrowsingBridge.setSafeBrowsingState(safeBrowsingState));
    }

    private void setCookieControlsMode(@CookieControlsMode int cookieControlsMode) {
        runOnUiThreadBlocking(
                ()
                        -> UserPrefs.get(Profile.getLastUsedRegularProfile())
                                   .setInteger(PrefNames.COOKIE_CONTROLS_MODE, cookieControlsMode));
    }

    private void executeWhileCapturingIntents(Runnable func) {
        Intents.init();
        try {
            Intent intent = new Intent();
            Instrumentation.ActivityResult result =
                    new Instrumentation.ActivityResult(Activity.RESULT_OK, intent);
            intending(anyIntent()).respondWith(result);

            if (func != null) {
                func.run();
            }
        } finally {
            Intents.release();
        }
    }

    private void testButtonVisibility(int buttonTextId, boolean isVisible) {
        if (isVisible) {
            onView(withText(buttonTextId)).check(matches(isDisplayed()));
        } else {
            onView(withText(buttonTextId)).check(matches(not(isDisplayed())));
        }
    }

    private void testButtons(boolean nextVisible, boolean backVisible, boolean finishVisible) {
        testButtonVisibility(R.string.next, nextVisible);
        testButtonVisibility(R.string.back, backVisible);
        testButtonVisibility(R.string.privacy_guide_finish_button, finishVisible);
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testForwardNavigation() {
        launchPrivacyGuide();

        navigateFromWelcomeToMSBBCard();
        testButtons(true, false, false);

        navigateFromMSBBToHistorySyncCard();
        testButtons(true, true, false);

        navigateFromHistorySyncToSBCard();
        testButtons(true, true, false);

        navigateFromSBToCookiesCard();
        testButtons(false, true, true);
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testWelcomeCard_nextClickWelcomeUserAction() {
        launchPrivacyGuide();
        navigateFromWelcomeToMSBBCard();
        // Verify that the user action is emitted when the next button is clicked on the welcome
        // page
        assertTrue(mActionTester.getActions().contains("Settings.PrivacyGuide.NextClickWelcome"));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testWelcomeCard_nextNavigationHistogram() {
        launchPrivacyGuide();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(
                        NEXT_NAVIGATION_HISTOGRAM, PrivacyGuideInteractions.WELCOME_NEXT_BUTTON));

        navigateFromWelcomeToMSBBCard();

        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(
                        NEXT_NAVIGATION_HISTOGRAM, PrivacyGuideInteractions.WELCOME_NEXT_BUTTON));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testCompletionCard_nextClickCompletionUserAction() {
        launchPrivacyGuide();
        goToCompletionCard();

        // Complete page -> EXIT
        onView(withText(R.string.done)).perform(click());

        // Verify that the user action is emitted when the next/done button is clicked on the
        // completion card
        assertTrue(
                mActionTester.getActions().contains("Settings.PrivacyGuide.NextClickCompletion"));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testCompletionCard_nextNavigationHistogram() {
        launchPrivacyGuide();
        goToCompletionCard();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(NEXT_NAVIGATION_HISTOGRAM,
                        PrivacyGuideInteractions.COMPLETION_NEXT_BUTTON));

        // Complete page -> EXIT
        onView(withText(R.string.done)).perform(click());

        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(NEXT_NAVIGATION_HISTOGRAM,
                        PrivacyGuideInteractions.COMPLETION_NEXT_BUTTON));
    }

    @Test
    @MediumTest
    @Feature({"PrivacyGuide"})
    @Features.EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_3)
    @Features.DisableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4)
    public void testCompletionCard_PrivacySandboxLinkNavigation() {
        launchPrivacyGuide();
        goToCompletionCard();

        onViewWaiting(withId(R.id.ps_button)).perform(click());
        onViewWaiting(withText(R.string.privacy_sandbox_trials_title))
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @Feature({"PrivacyGuide"})
    @Features.EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4)
    @Features.DisableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_3)
    public void testCompletionCard_AdPrivacyLinkNavigation() {
        launchPrivacyGuide();
        goToCompletionCard();

        onViewWaiting(withId(R.id.ps_button)).perform(click());
        onViewWaiting(withText(R.string.ad_privacy_page_title)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @Feature({"PrivacyGuide"})
    public void testCompletionCard_AdPrivacyClickUserAction() {
        launchPrivacyGuide();
        goToCompletionCard();

        onViewWaiting(withId(R.id.ps_button)).perform(click());
        assertTrue(mActionTester.getActions().contains("Settings.PrivacyGuide.CompletionPSClick"));
    }

    @Test
    @MediumTest
    @Feature({"PrivacyGuide"})
    public void testCompletionCard_AdPrivacyClickHistogram() {
        launchPrivacyGuide();
        goToCompletionCard();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(ENTRY_EXIT_HISTOGRAM,
                        PrivacyGuideInteractions.PRIVACY_SANDBOX_COMPLETION_LINK));

        onViewWaiting(withId(R.id.ps_button)).perform(click());

        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(ENTRY_EXIT_HISTOGRAM,
                        PrivacyGuideInteractions.PRIVACY_SANDBOX_COMPLETION_LINK));
    }

    @Test
    @MediumTest
    @Feature({"PrivacyGuide"})
    public void testCompletionCard_WaaLinkNavigation() {
        launchPrivacyGuide();
        goToCompletionCard();

        executeWhileCapturingIntents(() -> {
            onViewWaiting(withId(R.id.waa_button)).perform(click());
            intended(IntentMatchers.hasData(
                    UrlConstants.GOOGLE_ACCOUNT_ACTIVITY_CONTROLS_FROM_PG_URL));
        });
    }

    @Test
    @MediumTest
    @Feature({"PrivacyGuide"})
    public void testCompletionCard_WaaClickUserAction() {
        launchPrivacyGuide();
        goToCompletionCard();

        executeWhileCapturingIntents(() -> onViewWaiting(withId(R.id.waa_button)).perform(click()));

        assertTrue(
                mActionTester.getActions().contains("Settings.PrivacyGuide.CompletionSWAAClick"));
    }

    @Test
    @MediumTest
    @Feature({"PrivacyGuide"})
    public void testCompletionCard_WaaClickHistogram() {
        launchPrivacyGuide();
        goToCompletionCard();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(
                        ENTRY_EXIT_HISTOGRAM, PrivacyGuideInteractions.SWAA_COMPLETION_LINK));

        executeWhileCapturingIntents(() -> onViewWaiting(withId(R.id.waa_button)).perform(click()));

        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(
                        ENTRY_EXIT_HISTOGRAM, PrivacyGuideInteractions.SWAA_COMPLETION_LINK));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testMSBBCard_nextClickMSBBUserAction() {
        launchPrivacyGuide();
        goToHistorySyncCard();

        assertTrue(mActionTester.getActions().contains("Settings.PrivacyGuide.NextClickMSBB"));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testMSBBCard_nextNavigationHistogram() {
        launchPrivacyGuide();
        navigateFromWelcomeToMSBBCard();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(
                        NEXT_NAVIGATION_HISTOGRAM, PrivacyGuideInteractions.MSBB_NEXT_BUTTON));

        navigateFromMSBBToHistorySyncCard();

        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(
                        NEXT_NAVIGATION_HISTOGRAM, PrivacyGuideInteractions.MSBB_NEXT_BUTTON));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testMSBBCard_offToOffSettingsStatesHistogram() {
        setMSBBState(false);
        launchPrivacyGuide();
        navigateFromWelcomeToMSBBCard();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.MSBB_OFF_TO_OFF));

        navigateFromMSBBToHistorySyncCard();

        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.MSBB_OFF_TO_OFF));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testMSBBCard_offToOnSettingsStatesHistogram() {
        setMSBBState(false);
        launchPrivacyGuide();
        navigateFromWelcomeToMSBBCard();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.MSBB_OFF_TO_ON));

        onView(withId(R.id.msbb_switch)).perform(click());
        navigateFromMSBBToHistorySyncCard();

        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.MSBB_OFF_TO_ON));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testMSBBCard_onToOffSettingsStatesHistogram() {
        setMSBBState(true);
        launchPrivacyGuide();
        navigateFromWelcomeToMSBBCard();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.MSBB_ON_TO_OFF));

        onView(withId(R.id.msbb_switch)).perform(click());
        navigateFromMSBBToHistorySyncCard();

        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.MSBB_ON_TO_OFF));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testMSBBCard_onToOnSettingsStatesHistogram() {
        setMSBBState(true);
        launchPrivacyGuide();
        navigateFromWelcomeToMSBBCard();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.MSBB_ON_TO_ON));

        navigateFromMSBBToHistorySyncCard();

        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.MSBB_ON_TO_ON));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testMSBBCard_nextButtonInitialMSBBStateIsSet() {
        launchPrivacyGuide();
        mSettingsActivityTestRule.getFragment().setPrivacyGuideMetricsDelegateForTesting(
                mPrivacyGuideMetricsDelegateMock);
        navigateFromWelcomeToMSBBCard();

        verify(mPrivacyGuideMetricsDelegateMock)
                .setInitialStateForCard(PrivacyGuideFragment.FragmentType.MSBB);

        mSettingsActivityTestRule.getFragment().setPrivacyGuideMetricsDelegateForTesting(null);
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testMSBBCard_backButtonInitialMSBBStateIsSet() {
        launchPrivacyGuide();
        mSettingsActivityTestRule.getFragment().setPrivacyGuideMetricsDelegateForTesting(
                mPrivacyGuideMetricsDelegateMock);

        goToHistorySyncCard();
        navigateFromHistorySyncToMSBBCard();

        verify(mPrivacyGuideMetricsDelegateMock, times(2))
                .setInitialStateForCard(PrivacyGuideFragment.FragmentType.MSBB);

        mSettingsActivityTestRule.getFragment().setPrivacyGuideMetricsDelegateForTesting(null);
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testHistorySyncCard_nextClickHistorySyncUserAction() {
        launchPrivacyGuide();
        mActionTester = new UserActionTester();
        goToSafeBrowsingCard();

        assertTrue(
                mActionTester.getActions().contains("Settings.PrivacyGuide.NextClickHistorySync"));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testHistorySyncCard_nextNavigationHistogram() {
        launchPrivacyGuide();
        goToHistorySyncCard();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(NEXT_NAVIGATION_HISTOGRAM,
                        PrivacyGuideInteractions.HISTORY_SYNC_NEXT_BUTTON));

        navigateFromHistorySyncToSBCard();

        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(NEXT_NAVIGATION_HISTOGRAM,
                        PrivacyGuideInteractions.HISTORY_SYNC_NEXT_BUTTON));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testHistorySyncCard_offToOffSettingsStatesHistogram() {
        setHistorySyncState(false);
        launchPrivacyGuide();
        goToHistorySyncCard();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.HISTORY_SYNC_OFF_TO_OFF));

        navigateFromHistorySyncToSBCard();

        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.HISTORY_SYNC_OFF_TO_OFF));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testHistorySyncCard_offToOnSettingsStatesHistogram() {
        setHistorySyncState(false);
        launchPrivacyGuide();
        goToHistorySyncCard();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.HISTORY_SYNC_OFF_TO_ON));

        onView(withId(R.id.history_sync_switch)).perform(click());
        navigateFromHistorySyncToSBCard();

        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.HISTORY_SYNC_OFF_TO_ON));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testHistorySyncCard_onToOffSettingsStatesHistogram() {
        setHistorySyncState(true);
        launchPrivacyGuide();
        goToHistorySyncCard();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.HISTORY_SYNC_ON_TO_OFF));

        onView(withId(R.id.history_sync_switch)).perform(click());
        navigateFromHistorySyncToSBCard();

        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.HISTORY_SYNC_ON_TO_OFF));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testHistorySyncCard_onToOnSettingsStatesHistogram() {
        setHistorySyncState(true);
        launchPrivacyGuide();
        goToHistorySyncCard();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.HISTORY_SYNC_ON_TO_ON));

        navigateFromHistorySyncToSBCard();

        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.HISTORY_SYNC_ON_TO_ON));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testHistorySyncCard_nextButtonInitialSyncStateIsSet() {
        launchPrivacyGuide();
        mSettingsActivityTestRule.getFragment().setPrivacyGuideMetricsDelegateForTesting(
                mPrivacyGuideMetricsDelegateMock);
        goToHistorySyncCard();

        verify(mPrivacyGuideMetricsDelegateMock)
                .setInitialStateForCard(PrivacyGuideFragment.FragmentType.HISTORY_SYNC);

        mSettingsActivityTestRule.getFragment().setPrivacyGuideMetricsDelegateForTesting(null);
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testHistorySyncCard_backButtonInitialSyncStateIsSet() {
        launchPrivacyGuide();
        mSettingsActivityTestRule.getFragment().setPrivacyGuideMetricsDelegateForTesting(
                mPrivacyGuideMetricsDelegateMock);

        goToSafeBrowsingCard();
        navigateFromSBToHistorySyncCard();

        verify(mPrivacyGuideMetricsDelegateMock, times(2))
                .setInitialStateForCard(PrivacyGuideFragment.FragmentType.HISTORY_SYNC);

        mSettingsActivityTestRule.getFragment().setPrivacyGuideMetricsDelegateForTesting(null);
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testSafeBrowsingCard_nextClickSafeBrowsingUserAction() {
        launchPrivacyGuide();
        goToCookiesCard();

        assertTrue(
                mActionTester.getActions().contains("Settings.PrivacyGuide.NextClickSafeBrowsing"));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testSafeBrowsingCard_nextNavigationHistogram() {
        launchPrivacyGuide();
        goToSafeBrowsingCard();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(NEXT_NAVIGATION_HISTOGRAM,
                        PrivacyGuideInteractions.SAFE_BROWSING_NEXT_BUTTON));

        navigateFromSBToCookiesCard();

        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(NEXT_NAVIGATION_HISTOGRAM,
                        PrivacyGuideInteractions.SAFE_BROWSING_NEXT_BUTTON));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testSafeBrowsingCard_standardToStandardSettingsStatesHistogram() {
        setSafeBrowsingState(SafeBrowsingState.STANDARD_PROTECTION);
        launchPrivacyGuide();
        goToSafeBrowsingCard();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.SAFE_BROWSING_STANDARD_TO_STANDARD));

        navigateFromSBToCookiesCard();

        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.SAFE_BROWSING_STANDARD_TO_STANDARD));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testSafeBrowsingCard_standardToEnhancedSettingsStatesHistogram() {
        setSafeBrowsingState(SafeBrowsingState.STANDARD_PROTECTION);
        launchPrivacyGuide();
        goToSafeBrowsingCard();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.SAFE_BROWSING_STANDARD_TO_ENHANCED));

        onView(withId(R.id.enhanced_option)).perform(click());
        navigateFromSBToCookiesCard();

        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.SAFE_BROWSING_STANDARD_TO_ENHANCED));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testSafeBrowsingCard_enhancedToEnhancedSettingsStatesHistogram() {
        setSafeBrowsingState(SafeBrowsingState.ENHANCED_PROTECTION);
        launchPrivacyGuide();
        goToSafeBrowsingCard();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.SAFE_BROWSING_ENHANCED_TO_ENHANCED));

        navigateFromSBToCookiesCard();

        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.SAFE_BROWSING_ENHANCED_TO_ENHANCED));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testSafeBrowsingCard_enhancedToStandardSettingsStatesHistogram() {
        setSafeBrowsingState(SafeBrowsingState.ENHANCED_PROTECTION);
        launchPrivacyGuide();
        goToSafeBrowsingCard();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.SAFE_BROWSING_ENHANCED_TO_STANDARD));

        onView(withId(R.id.standard_option)).perform(click());
        navigateFromSBToCookiesCard();

        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.SAFE_BROWSING_ENHANCED_TO_STANDARD));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testSafeBrowsingCard_nextButtonInitialSafeBrowsingStateIsSet() {
        launchPrivacyGuide();
        mSettingsActivityTestRule.getFragment().setPrivacyGuideMetricsDelegateForTesting(
                mPrivacyGuideMetricsDelegateMock);
        goToSafeBrowsingCard();

        verify(mPrivacyGuideMetricsDelegateMock)
                .setInitialStateForCard(PrivacyGuideFragment.FragmentType.SAFE_BROWSING);

        mSettingsActivityTestRule.getFragment().setPrivacyGuideMetricsDelegateForTesting(null);
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testSafeBrowsingCard_backButtonInitialSafeBrowsingStateIsSet() {
        launchPrivacyGuide();
        mSettingsActivityTestRule.getFragment().setPrivacyGuideMetricsDelegateForTesting(
                mPrivacyGuideMetricsDelegateMock);

        goToCookiesCard();
        navigateFromCookiesToSBCard();

        verify(mPrivacyGuideMetricsDelegateMock, times(2))
                .setInitialStateForCard(PrivacyGuideFragment.FragmentType.SAFE_BROWSING);

        mSettingsActivityTestRule.getFragment().setPrivacyGuideMetricsDelegateForTesting(null);
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testCookiesCard_nextClickCookiesUserAction() {
        launchPrivacyGuide();
        goToCompletionCard();

        assertTrue(mActionTester.getActions().contains("Settings.PrivacyGuide.NextClickCookies"));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testCookiesCard_nextNavigationHistogram() {
        launchPrivacyGuide();
        goToCookiesCard();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(
                        NEXT_NAVIGATION_HISTOGRAM, PrivacyGuideInteractions.COOKIES_NEXT_BUTTON));

        navigateFromCookiesToCompletionCard();

        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(
                        NEXT_NAVIGATION_HISTOGRAM, PrivacyGuideInteractions.COOKIES_NEXT_BUTTON));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testCookiesCard_block3PIncognitoTo3PIncognitoSettingsStatesHistogram() {
        setCookieControlsMode(CookieControlsMode.INCOGNITO_ONLY);
        launchPrivacyGuide();
        goToCookiesCard();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.BLOCK3P_INCOGNITO_TO3P_INCOGNITO));

        navigateFromCookiesToCompletionCard();

        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.BLOCK3P_INCOGNITO_TO3P_INCOGNITO));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testCookiesCard_block3PIncognitoTo3PSettingsStatesHistogram() {
        setCookieControlsMode(CookieControlsMode.INCOGNITO_ONLY);
        launchPrivacyGuide();
        goToCookiesCard();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.BLOCK3P_INCOGNITO_TO3P));

        onView(withId(R.id.block_third_party)).perform(click());
        navigateFromCookiesToCompletionCard();

        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.BLOCK3P_INCOGNITO_TO3P));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testCookiesCard_block3PTo3PIncognitoSettingsStatesHistogram() {
        setCookieControlsMode(CookieControlsMode.BLOCK_THIRD_PARTY);
        launchPrivacyGuide();
        goToCookiesCard();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.BLOCK3P_TO3P_INCOGNITO));

        onView(withId(R.id.block_third_party_incognito)).perform(click());
        navigateFromCookiesToCompletionCard();

        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.BLOCK3P_TO3P_INCOGNITO));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testCookiesCard_block3PTo3PSettingsStatesHistogram() {
        setCookieControlsMode(CookieControlsMode.BLOCK_THIRD_PARTY);
        launchPrivacyGuide();
        goToCookiesCard();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.BLOCK3P_TO3P));

        navigateFromCookiesToCompletionCard();

        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.BLOCK3P_TO3P));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testCookiesCard_nextButtonInitialCookiesStateIsSet() {
        launchPrivacyGuide();
        mSettingsActivityTestRule.getFragment().setPrivacyGuideMetricsDelegateForTesting(
                mPrivacyGuideMetricsDelegateMock);
        goToCookiesCard();

        verify(mPrivacyGuideMetricsDelegateMock)
                .setInitialStateForCard(PrivacyGuideFragment.FragmentType.COOKIES);

        mSettingsActivityTestRule.getFragment().setPrivacyGuideMetricsDelegateForTesting(null);
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testHistorySyncCard_backClickHistorySyncUserAction() {
        launchPrivacyGuide();

        goToHistorySyncCard();
        navigateFromHistorySyncToMSBBCard();

        // Verify that the user action is emitted when the back button is clicked on the History
        // Sync card
        assertTrue(
                mActionTester.getActions().contains("Settings.PrivacyGuide.BackClickHistorySync"));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testSafeBrowsingCard_backClickSafeBrowsingUserAction() {
        launchPrivacyGuide();

        goToSafeBrowsingCard();
        navigateFromSBToHistorySyncCard();

        // Verify that the user action is emitted when the back button is clicked on the safe
        // browsing card
        assertTrue(
                mActionTester.getActions().contains("Settings.PrivacyGuide.BackClickSafeBrowsing"));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testCookiesCard_backClickCookiesUserAction() {
        launchPrivacyGuide();

        goToCookiesCard();
        navigateFromCookiesToSBCard();

        // Verify that the user action is emitted when the back button is clicked on the Cookies
        // card
        assertTrue(mActionTester.getActions().contains("Settings.PrivacyGuide.BackClickCookies"));
    }
}
