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
import org.chromium.components.sync.UserSelectableType;
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
    }

    private void launchPrivacyGuide() {
        mSettingsActivityTestRule.startSettingsActivity();
        ViewUtils.onViewWaiting(withText(R.string.prefs_privacy_guide_title));
    }

    private void navigateToSyncCard() {
        // Welcome page -> MSBB page
        onView(withText(R.string.privacy_guide_welcome_title)).check(matches(isDisplayed()));
        onView(withText(R.string.privacy_guide_start_button)).perform(click());

        // MSBB page -> Sync page
        ViewUtils.waitForView(withText(R.string.url_keyed_anonymized_data_title));
        onView(withText(R.string.next)).perform(click());
    }

    private void navigateToSafeBrowsingCard() {
        // Welcome page -> MSBB page
        onView(withText(R.string.privacy_guide_welcome_title)).check(matches(isDisplayed()));
        onView(withText(R.string.privacy_guide_start_button)).perform(click());

        // MSBB page -> Sync page
        ViewUtils.waitForView(withText(R.string.url_keyed_anonymized_data_title));
        onView(withText(R.string.next)).perform(click());

        // Sync page -> SB page
        ViewUtils.waitForView(withText(R.string.privacy_guide_sync_toggle));
        onView(withText(R.string.next)).perform(click());
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

        // MSBB page -> Sync page
        ViewUtils.waitForView(withText(R.string.url_keyed_anonymized_data_title));
        testButtons(true, false, false);
        onView(withText(R.string.next)).perform(click());

        // Sync page -> SB page
        ViewUtils.waitForView(withText(R.string.privacy_guide_sync_toggle));
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

        // MSBB page -> Sync page
        ViewUtils.waitForView(withText(R.string.url_keyed_anonymized_data_title));
        onView(withText(R.string.next)).perform(click());
        // Sync page -> SB page
        ViewUtils.waitForView(withText(R.string.privacy_guide_sync_toggle));
        onView(withText(R.string.next)).perform(click());
        // SB page -> Cookies page
        ViewUtils.waitForView(withText(R.string.privacy_guide_safe_browsing_intro));
        onView(withText(R.string.next)).perform(click());

        // SB page <- Cookies page
        ViewUtils.waitForView(withText(R.string.privacy_guide_cookies_intro));
        testButtons(false, true, true);
        onView(withText(R.string.back)).perform(click());
        // Sync page <- SB page
        ViewUtils.waitForView(withText(R.string.privacy_guide_safe_browsing_intro));
        testButtons(true, true, false);
        onView(withText(R.string.back)).perform(click());
        // MSBB page <- Sync page
        ViewUtils.waitForView(withText(R.string.privacy_guide_sync_toggle));
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
    public void testCompletionCard_nextClickCompletionUserAction() {
        launchPrivacyGuide();
        mActionTester = new UserActionTester();
        // Welcome page -> MSBB page
        onView(withText(R.string.privacy_guide_welcome_title)).check(matches(isDisplayed()));
        onView(withText(R.string.privacy_guide_start_button)).perform(click());

        // MSBB page -> Sync page
        ViewUtils.waitForView(withText(R.string.url_keyed_anonymized_data_title));
        onView(withText(R.string.next)).perform(click());

        // Sync page -> SB page
        ViewUtils.waitForView(withText(R.string.privacy_guide_sync_toggle));
        onView(withText(R.string.next)).perform(click());

        // SB page -> Cookies page
        ViewUtils.waitForView(withText(R.string.privacy_guide_safe_browsing_intro));
        onView(withText(R.string.next)).perform(click());

        // Cookies page -> Complete page
        ViewUtils.waitForView(withText(R.string.privacy_guide_cookies_intro));
        onView(withText(R.string.privacy_guide_finish_button)).perform(click());

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
    public void testMSBBCard_nextClickMSBBUserAction() {
        launchPrivacyGuide();
        mActionTester = new UserActionTester();
        // Welcome page -> MSBB page
        onView(withText(R.string.privacy_guide_welcome_title)).check(matches(isDisplayed()));
        onView(withText(R.string.privacy_guide_start_button)).perform(click());

        // MSBB page -> Sync page
        ViewUtils.waitForView(withText(R.string.url_keyed_anonymized_data_title));
        onView(withText(R.string.next)).perform(click());

        assertTrue(mActionTester.getActions().contains("Settings.PrivacyGuide.NextClickMSBB"));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testMSBBCard_offToOffSettingsStatesHistogram() {
        launchPrivacyGuide();
        setMSBBState(false);
        // Welcome page -> MSBB page
        onView(withText(R.string.privacy_guide_welcome_title)).check(matches(isDisplayed()));
        onView(withText(R.string.privacy_guide_start_button)).perform(click());

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.MSBB_OFF_TO_OFF));

        // MSBB page -> Sync page
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
        // Welcome page -> MSBB page
        onView(withText(R.string.privacy_guide_welcome_title)).check(matches(isDisplayed()));
        onView(withText(R.string.privacy_guide_start_button)).perform(click());

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.MSBB_OFF_TO_ON));

        // MSBB page -> Sync page | with click on MSBB switch
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
        // Welcome page -> MSBB page
        onView(withText(R.string.privacy_guide_welcome_title)).check(matches(isDisplayed()));
        onView(withText(R.string.privacy_guide_start_button)).perform(click());

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.MSBB_ON_TO_OFF));

        // MSBB page -> Sync page | with click on MSBB switch
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
        // Welcome page -> MSBB page
        onView(withText(R.string.privacy_guide_welcome_title)).check(matches(isDisplayed()));
        onView(withText(R.string.privacy_guide_start_button)).perform(click());

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.MSBB_ON_TO_ON));

        // MSBB page -> Sync page
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

        // Welcome page -> MSBB page
        onView(withText(R.string.privacy_guide_welcome_title)).check(matches(isDisplayed()));
        onView(withText(R.string.privacy_guide_start_button)).perform(click());

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

        // Welcome page -> MSBB page
        onView(withText(R.string.privacy_guide_welcome_title)).check(matches(isDisplayed()));
        onView(withText(R.string.privacy_guide_start_button)).perform(click());

        // MSBB page -> Sync page
        ViewUtils.waitForView(withText(R.string.url_keyed_anonymized_data_title));
        onView(withText(R.string.next)).perform(click());

        // MSBB page <- Sync page
        ViewUtils.waitForView(withText(R.string.privacy_guide_sync_toggle));
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
    public void testSyncCard_nextClickHistorySyncUserAction() {
        launchPrivacyGuide();
        mActionTester = new UserActionTester();
        navigateToSyncCard();

        // Sync page -> SB page
        ViewUtils.waitForView(withText(R.string.privacy_guide_sync_toggle));
        onView(withText(R.string.next)).perform(click());

        assertTrue(
                mActionTester.getActions().contains("Settings.PrivacyGuide.NextClickHistorySync"));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testSyncCard_offToOffSettingsStatesHistogram() {
        launchPrivacyGuide();
        navigateToSyncCard();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.HISTORY_SYNC_OFF_TO_OFF));

        // Sync page -> SB page
        ViewUtils.waitForView(withText(R.string.privacy_guide_sync_toggle));
        onView(withText(R.string.next)).perform(click());

        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.HISTORY_SYNC_OFF_TO_OFF));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testSyncCard_offToOnSettingsStatesHistogram() {
        launchPrivacyGuide();
        navigateToSyncCard();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.HISTORY_SYNC_OFF_TO_ON));

        // Sync page -> SB page | with click on sync switch
        ViewUtils.waitForView(withText(R.string.privacy_guide_sync_toggle));
        onView(withId(R.id.sync_switch)).perform(click());
        setHistorySyncState(true);
        onView(withText(R.string.next)).perform(click());

        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.HISTORY_SYNC_OFF_TO_ON));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testSyncCard_onToOffSettingsStatesHistogram() {
        launchPrivacyGuide();
        setHistorySyncState(true);
        navigateToSyncCard();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.HISTORY_SYNC_ON_TO_OFF));

        // Sync page -> SB page | with click on sync switch
        ViewUtils.waitForView(withText(R.string.privacy_guide_sync_toggle));
        onView(withId(R.id.sync_switch)).perform(click());
        setHistorySyncState(false);
        onView(withText(R.string.next)).perform(click());

        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.HISTORY_SYNC_ON_TO_OFF));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testSyncCard_onToOnSettingsStatesHistogram() {
        launchPrivacyGuide();
        setHistorySyncState(true);
        navigateToSyncCard();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.HISTORY_SYNC_ON_TO_ON));

        // Sync page -> SB page
        ViewUtils.waitForView(withText(R.string.privacy_guide_sync_toggle));
        onView(withText(R.string.next)).perform(click());

        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.HISTORY_SYNC_ON_TO_ON));
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testSyncCard_nextButtonInitialSyncStateIsSet() {
        launchPrivacyGuide();
        mSettingsActivityTestRule.getFragment().setPrivacyGuideMetricsDelegateForTesting(
                mPrivacyGuideMetricsDelegateMock);
        navigateToSyncCard();

        // Sync page
        ViewUtils.waitForView(withText(R.string.privacy_guide_sync_toggle));

        verify(mPrivacyGuideMetricsDelegateMock)
                .setInitialStateForCard(PrivacyGuideFragment.FragmentType.SYNC);

        mSettingsActivityTestRule.getFragment().setPrivacyGuideMetricsDelegateForTesting(null);
    }

    @Test
    @SmallTest
    @Feature({"PrivacyGuide"})
    public void testSyncCard_backButtonInitialSyncStateIsSet() {
        launchPrivacyGuide();
        mSettingsActivityTestRule.getFragment().setPrivacyGuideMetricsDelegateForTesting(
                mPrivacyGuideMetricsDelegateMock);
        navigateToSyncCard();

        // Sync page -> SB page
        ViewUtils.waitForView(withText(R.string.privacy_guide_sync_toggle));
        onView(withText(R.string.next)).perform(click());

        // Sync page <- SB page
        ViewUtils.waitForView(withText(R.string.privacy_guide_safe_browsing_intro));
        onView(withText(R.string.back)).perform(click());

        // Sync page
        ViewUtils.waitForView(withText(R.string.privacy_guide_sync_toggle));

        verify(mPrivacyGuideMetricsDelegateMock, times(2))
                .setInitialStateForCard(PrivacyGuideFragment.FragmentType.SYNC);

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
    public void testSyncCard_backClickHistorySyncUserAction() {
        launchPrivacyGuide();
        mActionTester = new UserActionTester();
        navigateToSyncCard();

        // MSBB page <- Sync page
        ViewUtils.waitForView(withText(R.string.privacy_guide_sync_toggle));
        onView(withText(R.string.back)).perform(click());

        // Verify that the user action is emitted when the back button is clicked on the History
        // sync card
        assertTrue(
                mActionTester.getActions().contains("Settings.PrivacyGuide.BackClickHistorySync"));
    }
}
