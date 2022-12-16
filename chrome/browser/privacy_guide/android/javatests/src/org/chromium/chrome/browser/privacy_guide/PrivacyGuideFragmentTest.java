// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

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
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridge;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.content_settings.CookieControlsMode;
import org.chromium.components.content_settings.PrefNames;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.ViewUtils;

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

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public SettingsActivityTestRule<PrivacyGuideFragment> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(PrivacyGuideFragment.class);

    @Rule
    public HistogramTestRule mHistogramTestRule = new HistogramTestRule();

    @Mock
    private SyncService mSyncService;
    @Mock
    private PrivacyGuideMetricsDelegate mPrivacyGuideMetricsDelegateMock;
    @Mock
    private Set<Integer> mSyncTypes;

    private UserActionTester mActionTester;
    private boolean mIsHistorySyncOn;

    @BeforeClass
    public static void setUpBeforeActivityLaunched() {
        // Only needs to be loaded once and needs to be loaded before HistogramTestRule.
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
    }

    @Before
    public void setUp() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SyncService.overrideForTests(mSyncService);
            when(mSyncService.isSyncFeatureEnabled()).thenReturn(true);
            when(mSyncService.getSelectedTypes()).thenReturn(mSyncTypes);
            when(mSyncTypes.contains(UserSelectableType.HISTORY)).thenAnswer(i -> mIsHistorySyncOn);
        });
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { SyncService.resetForTests(); });
        if (mActionTester != null) mActionTester.tearDown();
    }

    private void launchPrivacyGuide() {
        mSettingsActivityTestRule.startSettingsActivity();
        ViewUtils.onViewWaiting(withText(R.string.prefs_privacy_guide_title));
    }

    private void navigateToMSBBCard() {
        // Welcome page -> MSBB page
        onView(withText(R.string.privacy_guide_welcome_title)).check(matches(isDisplayed()));
        onView(withText(R.string.privacy_guide_start_button)).perform(click());
    }

    private void navigateToHistorySyncCard() {
        // Welcome page -> MSBB page
        onView(withText(R.string.privacy_guide_welcome_title)).check(matches(isDisplayed()));
        onView(withText(R.string.privacy_guide_start_button)).perform(click());

        // MSBB page -> History Sync page
        ViewUtils.waitForView(withText(R.string.url_keyed_anonymized_data_title));
        onView(withText(R.string.next)).perform(click());
    }

    private void navigateToSafeBrowsingCard() {
        // Welcome page -> MSBB page
        onView(withText(R.string.privacy_guide_welcome_title)).check(matches(isDisplayed()));
        onView(withText(R.string.privacy_guide_start_button)).perform(click());

        // MSBB page -> History Sync page
        ViewUtils.waitForView(withText(R.string.url_keyed_anonymized_data_title));
        onView(withText(R.string.next)).perform(click());

        // History Sync page -> SB page
        ViewUtils.waitForView(withText(R.string.privacy_guide_history_sync_toggle));
        onView(withText(R.string.next)).perform(click());
    }

    private void navigateToCookiesCard() {
        // Welcome page -> MSBB page
        onView(withText(R.string.privacy_guide_welcome_title)).check(matches(isDisplayed()));
        onView(withText(R.string.privacy_guide_start_button)).perform(click());

        // MSBB page -> History Sync page
        ViewUtils.waitForView(withText(R.string.url_keyed_anonymized_data_title));
        onView(withText(R.string.next)).perform(click());

        // History Sync page -> SB page
        ViewUtils.waitForView(withText(R.string.privacy_guide_history_sync_toggle));
        onView(withText(R.string.next)).perform(click());

        // SB page -> Cookies page
        ViewUtils.waitForView(withText(R.string.privacy_guide_safe_browsing_intro));
        onView(withText(R.string.next)).perform(click());
    }

    private void navigateToCompletionCard() {
        // Welcome page -> MSBB page
        onView(withText(R.string.privacy_guide_welcome_title)).check(matches(isDisplayed()));
        onView(withText(R.string.privacy_guide_start_button)).perform(click());

        // MSBB page -> History Sync page
        ViewUtils.waitForView(withText(R.string.url_keyed_anonymized_data_title));
        onView(withText(R.string.next)).perform(click());

        // History Sync page -> SB page
        ViewUtils.waitForView(withText(R.string.privacy_guide_history_sync_toggle));
        onView(withText(R.string.next)).perform(click());

        // SB page -> Cookies page
        ViewUtils.waitForView(withText(R.string.privacy_guide_safe_browsing_intro));
        onView(withText(R.string.next)).perform(click());

        // Cookies page -> Complete page
        ViewUtils.waitForView(withText(R.string.privacy_guide_cookies_intro));
        onView(withText(R.string.privacy_guide_finish_button)).perform(click());
    }

    private void setMSBBState(boolean isMSBBon) {
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(
                                Profile.getLastUsedRegularProfile(), isMSBBon));
    }

    private void setHistorySyncState(boolean isHistorySyncOn) {
        mIsHistorySyncOn = isHistorySyncOn;
    }

    private void setSafeBrowsingState(@SafeBrowsingState int safeBrowsingState) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> SafeBrowsingBridge.setSafeBrowsingState(safeBrowsingState));
    }

    private void setCookieControlsMode(@CookieControlsMode int cookieControlsMode) {
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> UserPrefs.get(Profile.getLastUsedRegularProfile())
                                   .setInteger(PrefNames.COOKIE_CONTROLS_MODE, cookieControlsMode));
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
        // Welcome page -> MSBB page
        onView(withText(R.string.privacy_guide_welcome_title)).check(matches(isDisplayed()));
        onView(withText(R.string.privacy_guide_start_button)).perform(click());

        // MSBB page -> History Sync page
        ViewUtils.waitForView(withText(R.string.url_keyed_anonymized_data_title));
        testButtons(true, false, false);
        onView(withText(R.string.next)).perform(click());

        // History Sync page -> SB page
        ViewUtils.waitForView(withText(R.string.privacy_guide_history_sync_toggle));
        testButtons(true, true, false);
        onView(withText(R.string.next)).perform(click());

        // SB page -> Cookies page
        ViewUtils.waitForView(withText(R.string.privacy_guide_safe_browsing_intro));
        testButtons(true, true, false);
        onView(withText(R.string.next)).perform(click());

        // Cookies page -> Complete page
        ViewUtils.waitForView(withText(R.string.privacy_guide_cookies_intro));
        testButtons(false, true, true);
        onView(withText(R.string.privacy_guide_finish_button)).perform(click());

        // Complete page -> EXIT
        ViewUtils.waitForView(withText(R.string.privacy_guide_done_title));
        onView(withText(R.string.done)).perform(click());
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testBackwardNavigation() {
        launchPrivacyGuide();
        // Welcome page -> MSBB page
        onView(withText(R.string.privacy_guide_start_button)).perform(click());

        // MSBB page -> History Sync page
        ViewUtils.waitForView(withText(R.string.url_keyed_anonymized_data_title));
        onView(withText(R.string.next)).perform(click());
        // History Sync page -> SB page
        ViewUtils.waitForView(withText(R.string.privacy_guide_history_sync_toggle));
        onView(withText(R.string.next)).perform(click());
        // SB page -> Cookies page
        ViewUtils.waitForView(withText(R.string.privacy_guide_safe_browsing_intro));
        onView(withText(R.string.next)).perform(click());

        // SB page <- Cookies page
        ViewUtils.waitForView(withText(R.string.privacy_guide_cookies_intro));
        testButtons(false, true, true);
        onView(withText(R.string.back)).perform(click());
        // History Sync page <- SB page
        ViewUtils.waitForView(withText(R.string.privacy_guide_safe_browsing_intro));
        testButtons(true, true, false);
        onView(withText(R.string.back)).perform(click());
        // MSBB page <- History Sync page
        ViewUtils.waitForView(withText(R.string.privacy_guide_history_sync_toggle));
        testButtons(true, true, false);
        onView(withText(R.string.back)).perform(click());
        // MSBB page -> Exit
        ViewUtils.waitForView(withText(R.string.url_keyed_anonymized_data_title));
        testButtons(true, false, false);
        onView(withId(R.id.close_menu_id)).perform(click());
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testWelcomeCard_nextClickWelcomeUserAction() {
        launchPrivacyGuide();
        mActionTester = new UserActionTester();
        // Welcome page -> MSBB page
        onView(withText(R.string.privacy_guide_welcome_title)).check(matches(isDisplayed()));
        onView(withText(R.string.privacy_guide_start_button)).perform(click());
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

        // Welcome page -> MSBB page
        onView(withText(R.string.privacy_guide_welcome_title)).check(matches(isDisplayed()));
        onView(withText(R.string.privacy_guide_start_button)).perform(click());

        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(
                        NEXT_NAVIGATION_HISTOGRAM, PrivacyGuideInteractions.WELCOME_NEXT_BUTTON));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testCompletionCard_nextClickCompletionUserAction() {
        launchPrivacyGuide();
        mActionTester = new UserActionTester();
        navigateToCompletionCard();

        // Complete page -> EXIT
        ViewUtils.waitForView(withText(R.string.privacy_guide_done_title));
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
        navigateToCompletionCard();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(NEXT_NAVIGATION_HISTOGRAM,
                        PrivacyGuideInteractions.COMPLETION_NEXT_BUTTON));

        // Complete page -> EXIT
        ViewUtils.waitForView(withText(R.string.privacy_guide_done_title));
        onView(withText(R.string.done)).perform(click());

        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(NEXT_NAVIGATION_HISTOGRAM,
                        PrivacyGuideInteractions.COMPLETION_NEXT_BUTTON));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testMSBBCard_nextClickMSBBUserAction() {
        launchPrivacyGuide();
        mActionTester = new UserActionTester();
        navigateToMSBBCard();

        // MSBB page -> History Sync page
        ViewUtils.waitForView(withText(R.string.url_keyed_anonymized_data_title));
        onView(withText(R.string.next)).perform(click());

        assertTrue(mActionTester.getActions().contains("Settings.PrivacyGuide.NextClickMSBB"));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testMSBBCard_nextNavigationHistogram() {
        launchPrivacyGuide();
        navigateToMSBBCard();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(
                        NEXT_NAVIGATION_HISTOGRAM, PrivacyGuideInteractions.MSBB_NEXT_BUTTON));

        // MSBB page -> History Sync page
        ViewUtils.waitForView(withText(R.string.url_keyed_anonymized_data_title));
        onView(withText(R.string.next)).perform(click());

        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(
                        NEXT_NAVIGATION_HISTOGRAM, PrivacyGuideInteractions.MSBB_NEXT_BUTTON));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testMSBBCard_offToOffSettingsStatesHistogram() {
        launchPrivacyGuide();
        setMSBBState(false);
        navigateToMSBBCard();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.MSBB_OFF_TO_OFF));

        // MSBB page -> History Sync page
        ViewUtils.waitForView(withText(R.string.url_keyed_anonymized_data_title));
        onView(withText(R.string.next)).perform(click());

        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.MSBB_OFF_TO_OFF));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testMSBBCard_offToOnSettingsStatesHistogram() {
        launchPrivacyGuide();
        setMSBBState(false);
        navigateToMSBBCard();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.MSBB_OFF_TO_ON));

        // MSBB page -> History Sync page | with click on MSBB switch
        ViewUtils.waitForView(withText(R.string.url_keyed_anonymized_data_title));
        onView(withId(R.id.msbb_switch)).perform(click());
        onView(withText(R.string.next)).perform(click());

        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.MSBB_OFF_TO_ON));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testMSBBCard_onToOffSettingsStatesHistogram() {
        launchPrivacyGuide();
        setMSBBState(true);
        navigateToMSBBCard();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.MSBB_ON_TO_OFF));

        // MSBB page -> History Sync page | with click on MSBB switch
        ViewUtils.waitForView(withText(R.string.url_keyed_anonymized_data_title));
        onView(withId(R.id.msbb_switch)).perform(click());
        onView(withText(R.string.next)).perform(click());

        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.MSBB_ON_TO_OFF));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testMSBBCard_onToOnSettingsStatesHistogram() {
        launchPrivacyGuide();
        setMSBBState(true);
        navigateToMSBBCard();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.MSBB_ON_TO_ON));

        // MSBB page -> History Sync page
        ViewUtils.waitForView(withText(R.string.url_keyed_anonymized_data_title));
        onView(withText(R.string.next)).perform(click());

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
        navigateToMSBBCard();

        // MSBB page
        ViewUtils.waitForView(withText(R.string.url_keyed_anonymized_data_title));

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
        navigateToMSBBCard();

        // MSBB page -> History Sync page
        ViewUtils.waitForView(withText(R.string.url_keyed_anonymized_data_title));
        onView(withText(R.string.next)).perform(click());

        // MSBB page <- History Sync page
        ViewUtils.waitForView(withText(R.string.privacy_guide_history_sync_toggle));
        onView(withText(R.string.back)).perform(click());

        // MSBB page
        ViewUtils.waitForView(withText(R.string.url_keyed_anonymized_data_title));

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
        navigateToHistorySyncCard();

        // History Sync page -> SB page
        ViewUtils.waitForView(withText(R.string.privacy_guide_history_sync_toggle));
        onView(withText(R.string.next)).perform(click());

        assertTrue(
                mActionTester.getActions().contains("Settings.PrivacyGuide.NextClickHistorySync"));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testHistorySyncCard_nextNavigationHistogram() {
        launchPrivacyGuide();
        navigateToHistorySyncCard();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(NEXT_NAVIGATION_HISTOGRAM,
                        PrivacyGuideInteractions.HISTORY_SYNC_NEXT_BUTTON));

        // History Sync page -> SB page
        ViewUtils.waitForView(withText(R.string.privacy_guide_history_sync_toggle));
        onView(withText(R.string.next)).perform(click());

        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(NEXT_NAVIGATION_HISTOGRAM,
                        PrivacyGuideInteractions.HISTORY_SYNC_NEXT_BUTTON));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testHistorySyncCard_offToOffSettingsStatesHistogram() {
        launchPrivacyGuide();
        navigateToHistorySyncCard();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.HISTORY_SYNC_OFF_TO_OFF));

        // History Sync page -> SB page
        ViewUtils.waitForView(withText(R.string.privacy_guide_history_sync_toggle));
        onView(withText(R.string.next)).perform(click());

        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.HISTORY_SYNC_OFF_TO_OFF));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testHistorySyncCard_offToOnSettingsStatesHistogram() {
        launchPrivacyGuide();
        navigateToHistorySyncCard();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.HISTORY_SYNC_OFF_TO_ON));

        // History Sync page -> SB page | with click on History Sync switch
        ViewUtils.waitForView(withText(R.string.privacy_guide_history_sync_toggle));
        onView(withId(R.id.history_sync_switch)).perform(click());
        setHistorySyncState(true);
        onView(withText(R.string.next)).perform(click());

        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.HISTORY_SYNC_OFF_TO_ON));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testHistorySyncCard_onToOffSettingsStatesHistogram() {
        launchPrivacyGuide();
        setHistorySyncState(true);
        navigateToHistorySyncCard();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.HISTORY_SYNC_ON_TO_OFF));

        // History Sync page -> SB page | with click on History Sync switch
        ViewUtils.waitForView(withText(R.string.privacy_guide_history_sync_toggle));
        onView(withId(R.id.history_sync_switch)).perform(click());
        setHistorySyncState(false);
        onView(withText(R.string.next)).perform(click());

        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.HISTORY_SYNC_ON_TO_OFF));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testHistorySyncCard_onToOnSettingsStatesHistogram() {
        launchPrivacyGuide();
        setHistorySyncState(true);
        navigateToHistorySyncCard();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.HISTORY_SYNC_ON_TO_ON));

        // History Sync page -> SB page
        ViewUtils.waitForView(withText(R.string.privacy_guide_history_sync_toggle));
        onView(withText(R.string.next)).perform(click());

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
        navigateToHistorySyncCard();

        // History Sync page
        ViewUtils.waitForView(withText(R.string.privacy_guide_history_sync_toggle));

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
        navigateToHistorySyncCard();

        // History Sync page -> SB page
        ViewUtils.waitForView(withText(R.string.privacy_guide_history_sync_toggle));
        onView(withText(R.string.next)).perform(click());

        // History Sync page <- SB page
        ViewUtils.waitForView(withText(R.string.privacy_guide_safe_browsing_intro));
        onView(withText(R.string.back)).perform(click());

        // History Sync page
        ViewUtils.waitForView(withText(R.string.privacy_guide_history_sync_toggle));

        verify(mPrivacyGuideMetricsDelegateMock, times(2))
                .setInitialStateForCard(PrivacyGuideFragment.FragmentType.HISTORY_SYNC);

        mSettingsActivityTestRule.getFragment().setPrivacyGuideMetricsDelegateForTesting(null);
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testSafeBrowsingCard_nextClickSafeBrowsingUserAction() {
        launchPrivacyGuide();
        mActionTester = new UserActionTester();
        navigateToSafeBrowsingCard();

        // SB page -> Cookies page
        ViewUtils.waitForView(withText(R.string.privacy_guide_safe_browsing_intro));
        onView(withText(R.string.next)).perform(click());

        assertTrue(
                mActionTester.getActions().contains("Settings.PrivacyGuide.NextClickSafeBrowsing"));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testSafeBrowsingCard_nextNavigationHistogram() {
        launchPrivacyGuide();
        navigateToSafeBrowsingCard();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(NEXT_NAVIGATION_HISTOGRAM,
                        PrivacyGuideInteractions.SAFE_BROWSING_NEXT_BUTTON));

        // SB page -> Cookies page
        ViewUtils.waitForView(withText(R.string.privacy_guide_safe_browsing_intro));
        onView(withText(R.string.next)).perform(click());

        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(NEXT_NAVIGATION_HISTOGRAM,
                        PrivacyGuideInteractions.SAFE_BROWSING_NEXT_BUTTON));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testSafeBrowsingCard_standardToStandardSettingsStatesHistogram() {
        launchPrivacyGuide();
        setSafeBrowsingState(SafeBrowsingState.STANDARD_PROTECTION);
        navigateToSafeBrowsingCard();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.SAFE_BROWSING_STANDARD_TO_STANDARD));

        // SB page -> Cookies page
        ViewUtils.waitForView(withText(R.string.privacy_guide_safe_browsing_intro));
        onView(withText(R.string.next)).perform(click());

        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.SAFE_BROWSING_STANDARD_TO_STANDARD));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testSafeBrowsingCard_standardToEnhancedSettingsStatesHistogram() {
        launchPrivacyGuide();
        setSafeBrowsingState(SafeBrowsingState.STANDARD_PROTECTION);
        navigateToSafeBrowsingCard();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.SAFE_BROWSING_STANDARD_TO_ENHANCED));

        // SB page -> Cookies page | with click on enhanced protection radio button
        ViewUtils.waitForView(withText(R.string.privacy_guide_safe_browsing_intro));
        onView(withId(R.id.enhanced_option)).perform(click());
        onView(withText(R.string.next)).perform(click());

        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.SAFE_BROWSING_STANDARD_TO_ENHANCED));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testSafeBrowsingCard_enhancedToEnhancedSettingsStatesHistogram() {
        launchPrivacyGuide();
        setSafeBrowsingState(SafeBrowsingState.ENHANCED_PROTECTION);
        navigateToSafeBrowsingCard();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.SAFE_BROWSING_ENHANCED_TO_ENHANCED));

        // SB page -> Cookies page
        ViewUtils.waitForView(withText(R.string.privacy_guide_safe_browsing_intro));
        onView(withText(R.string.next)).perform(click());

        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.SAFE_BROWSING_ENHANCED_TO_ENHANCED));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testSafeBrowsingCard_enhancedToStandardSettingsStatesHistogram() {
        launchPrivacyGuide();
        setSafeBrowsingState(SafeBrowsingState.ENHANCED_PROTECTION);
        navigateToSafeBrowsingCard();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.SAFE_BROWSING_ENHANCED_TO_STANDARD));

        // SB page -> Cookies page | with click on standard protection radio button
        ViewUtils.waitForView(withText(R.string.privacy_guide_safe_browsing_intro));
        onView(withId(R.id.standard_option)).perform(click());
        onView(withText(R.string.next)).perform(click());

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
        navigateToSafeBrowsingCard();

        // SB page
        ViewUtils.waitForView(withText(R.string.privacy_guide_safe_browsing_intro));

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
        navigateToSafeBrowsingCard();

        // SB page -> Cookies page
        ViewUtils.waitForView(withText(R.string.privacy_guide_safe_browsing_intro));
        onView(withText(R.string.next)).perform(click());

        // SB page <- Cookies page
        ViewUtils.waitForView(withText(R.string.privacy_guide_cookies_intro));
        onView(withText(R.string.back)).perform(click());

        // SB page
        ViewUtils.waitForView(withText(R.string.privacy_guide_safe_browsing_intro));

        verify(mPrivacyGuideMetricsDelegateMock, times(2))
                .setInitialStateForCard(PrivacyGuideFragment.FragmentType.SAFE_BROWSING);

        mSettingsActivityTestRule.getFragment().setPrivacyGuideMetricsDelegateForTesting(null);
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testCookiesCard_nextClickCookiesUserAction() {
        launchPrivacyGuide();
        mActionTester = new UserActionTester();
        navigateToCookiesCard();

        // Cookies page -> Complete page
        ViewUtils.waitForView(withText(R.string.privacy_guide_cookies_intro));
        onView(withText(R.string.privacy_guide_finish_button)).perform(click());

        assertTrue(mActionTester.getActions().contains("Settings.PrivacyGuide.NextClickCookies"));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testCookiesCard_nextNavigationHistogram() {
        launchPrivacyGuide();
        navigateToCookiesCard();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(
                        NEXT_NAVIGATION_HISTOGRAM, PrivacyGuideInteractions.COOKIES_NEXT_BUTTON));

        // Cookies page -> Complete page
        ViewUtils.waitForView(withText(R.string.privacy_guide_cookies_intro));
        onView(withText(R.string.privacy_guide_finish_button)).perform(click());

        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(
                        NEXT_NAVIGATION_HISTOGRAM, PrivacyGuideInteractions.COOKIES_NEXT_BUTTON));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testCookiesCard_block3PIncognitoTo3PIncognitoSettingsStatesHistogram() {
        launchPrivacyGuide();
        setCookieControlsMode(CookieControlsMode.INCOGNITO_ONLY);
        navigateToCookiesCard();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.BLOCK3P_INCOGNITO_TO3P_INCOGNITO));

        // Cookies page -> Complete page
        ViewUtils.waitForView(withText(R.string.privacy_guide_cookies_intro));
        onView(withText(R.string.privacy_guide_finish_button)).perform(click());

        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.BLOCK3P_INCOGNITO_TO3P_INCOGNITO));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testCookiesCard_block3PIncognitoTo3PSettingsStatesHistogram() {
        launchPrivacyGuide();
        setCookieControlsMode(CookieControlsMode.INCOGNITO_ONLY);
        navigateToCookiesCard();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.BLOCK3P_INCOGNITO_TO3P));

        // Cookies page -> Complete page | with click on block third party radio button
        ViewUtils.waitForView(withText(R.string.privacy_guide_cookies_intro));
        onView(withId(R.id.block_third_party)).perform(click());
        onView(withText(R.string.privacy_guide_finish_button)).perform(click());

        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.BLOCK3P_INCOGNITO_TO3P));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testCookiesCard_block3PTo3PIncognitoSettingsStatesHistogram() {
        launchPrivacyGuide();
        setCookieControlsMode(CookieControlsMode.BLOCK_THIRD_PARTY);
        navigateToCookiesCard();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.BLOCK3P_TO3P_INCOGNITO));

        // Cookies page -> Complete page | with click on block incognito radio button
        ViewUtils.waitForView(withText(R.string.privacy_guide_cookies_intro));
        onView(withId(R.id.block_third_party_incognito)).perform(click());
        onView(withText(R.string.privacy_guide_finish_button)).perform(click());

        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.BLOCK3P_TO3P_INCOGNITO));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testCookiesCard_block3PTo3PSettingsStatesHistogram() {
        launchPrivacyGuide();
        setCookieControlsMode(CookieControlsMode.BLOCK_THIRD_PARTY);
        navigateToCookiesCard();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.BLOCK3P_TO3P));

        // Cookies page -> Complete page
        ViewUtils.waitForView(withText(R.string.privacy_guide_cookies_intro));
        onView(withText(R.string.privacy_guide_finish_button)).perform(click());

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
        navigateToCookiesCard();

        // Cookies page
        ViewUtils.waitForView(withText(R.string.privacy_guide_cookies_intro));

        verify(mPrivacyGuideMetricsDelegateMock)
                .setInitialStateForCard(PrivacyGuideFragment.FragmentType.COOKIES);

        mSettingsActivityTestRule.getFragment().setPrivacyGuideMetricsDelegateForTesting(null);
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testHistorySyncCard_backClickHistorySyncUserAction() {
        launchPrivacyGuide();
        mActionTester = new UserActionTester();
        navigateToHistorySyncCard();

        // MSBB page <- History Sync page
        ViewUtils.waitForView(withText(R.string.privacy_guide_history_sync_toggle));
        onView(withText(R.string.back)).perform(click());

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
        mActionTester = new UserActionTester();
        navigateToSafeBrowsingCard();

        // History Sync page <- SB page
        ViewUtils.waitForView(withText(R.string.privacy_guide_safe_browsing_intro));
        onView(withText(R.string.back)).perform(click());

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
        mActionTester = new UserActionTester();
        navigateToCookiesCard();

        // SB page <- Cookies page
        ViewUtils.waitForView(withText(R.string.privacy_guide_cookies_intro));
        onView(withText(R.string.back)).perform(click());

        // Verify that the user action is emitted when the back button is clicked on the Cookies
        // card
        assertTrue(mActionTester.getActions().contains("Settings.PrivacyGuide.BackClickCookies"));
    }
}
