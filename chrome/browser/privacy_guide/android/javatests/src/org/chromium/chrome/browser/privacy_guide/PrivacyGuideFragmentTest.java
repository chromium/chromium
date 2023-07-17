// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.Espresso.pressBack;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.scrollTo;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.intent.Intents.intended;
import static androidx.test.espresso.intent.Intents.intending;
import static androidx.test.espresso.intent.matcher.IntentMatchers.anyIntent;
import static androidx.test.espresso.matcher.ViewMatchers.hasSibling;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withChild;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.not;
import static org.hamcrest.Matchers.allOf;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.Intent;
import android.view.View;

import androidx.test.espresso.intent.Intents;
import androidx.test.espresso.intent.matcher.IntentMatchers;
import androidx.test.filters.LargeTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.privacy.settings.PrivacySettings;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridge;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.content_settings.CookieControlsMode;
import org.chromium.components.content_settings.PrefNames;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.components.user_prefs.UserPrefs;

import java.io.IOException;
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
    public ChromeBrowserTestRule mChromeBrowserTestRule = new ChromeBrowserTestRule();

    @Rule
    public SettingsActivityTestRule<PrivacyGuideFragment> mPrivacyGuideTestRule =
            new SettingsActivityTestRule<>(PrivacyGuideFragment.class);

    @Rule
    public SettingsActivityTestRule<PrivacySettings> mPrivacySettingsTestRule =
            new SettingsActivityTestRule<>(PrivacySettings.class);

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.UI_SETTINGS_PRIVACY)
                    .build();

    @Mock
    private PrivacyGuideMetricsDelegate mPrivacyGuideMetricsDelegateMock;

    private UserActionTester mActionTester;

    @Before
    public void setUp() {
        mChromeBrowserTestRule.addTestAccountThenSigninAndEnableSync();
        mActionTester = new UserActionTester();
    }

    @After
    public void tearDown() {
        mActionTester.tearDown();
        mActionTester = null;
    }

    private void launchPrivacyGuide() {
        mPrivacyGuideTestRule.startSettingsActivity();
        onViewWaiting(withText(R.string.privacy_guide_fragment_title));
    }

    private void launchPrivacySettingsAndOpenPrivacyGuide() {
        mPrivacySettingsTestRule.startSettingsActivity();
        onViewWaiting(withText(R.string.privacy_guide_pref_summary)).perform(click());
        onViewWaiting(withText(R.string.privacy_guide_fragment_title));
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

        runOnUiThreadBlocking(
                () -> SyncServiceFactory.get().setSelectedTypes(false, selectedTypes));
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

    private View getRootView() {
        return mPrivacyGuideTestRule.getActivity().findViewById(android.R.id.content).getRootView();
    }

    private void clickOnArrowNextToRadioButtonWithText(int textId) {
        onView(allOf(withId(R.id.expand_arrow),
                       withParent(hasSibling(withChild(withText(textId))))))
                .perform(scrollTo(), click());
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testRenderWelcomeCard() throws IOException {
        launchPrivacyGuide();
        mRenderTestRule.render(getRootView(), "privacy_guide_welcome");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testRenderMSBBCard() throws IOException {
        launchPrivacyGuide();
        navigateFromWelcomeToMSBBCard();
        mRenderTestRule.render(getRootView(), "privacy_guide_msbb");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testRenderHistorySyncCard() throws IOException {
        launchPrivacyGuide();
        goToHistorySyncCard();
        mRenderTestRule.render(getRootView(), "privacy_guide_history_sync");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testRenderSBCard() throws IOException {
        launchPrivacyGuide();
        goToSafeBrowsingCard();
        mRenderTestRule.render(getRootView(), "privacy_guide_sb");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testRenderSBEnhancedBottomSheet() throws IOException {
        launchPrivacyGuide();
        goToSafeBrowsingCard();
        clickOnArrowNextToRadioButtonWithText(R.string.privacy_guide_safe_browsing_enhanced_title);
        mRenderTestRule.render(getRootView(), "privacy_guide_sb_enhanced_sheet");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testRenderSBStandardBottomSheet() throws IOException {
        launchPrivacyGuide();
        goToSafeBrowsingCard();
        clickOnArrowNextToRadioButtonWithText(R.string.privacy_guide_safe_browsing_standard_title);
        mRenderTestRule.render(getRootView(), "privacy_guide_sb_standard_sheet");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testRenderCookiesCard() throws IOException {
        launchPrivacyGuide();
        goToCookiesCard();
        mRenderTestRule.render(getRootView(), "privacy_guide_cookies");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testRenderCompletionCard() throws IOException {
        launchPrivacyGuide();
        goToCompletionCard();
        mRenderTestRule.render(getRootView(), "privacy_guide_completion");
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testForwardNavigation() {
        launchPrivacyGuide();
        testButtons(false, false, false);

        navigateFromWelcomeToMSBBCard();
        testButtons(true, true, false);

        navigateFromMSBBToHistorySyncCard();
        testButtons(true, true, false);

        navigateFromHistorySyncToSBCard();
        testButtons(true, true, false);

        navigateFromSBToCookiesCard();
        testButtons(false, true, true);

        navigateFromCookiesToCompletionCard();
        testButtons(false, false, false);
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    @Features.EnableFeatures(ChromeFeatureList.PRIVACY_GUIDE_POST_MVP)
    public void testBackButtonNavigation() {
        launchPrivacyGuide();
        goToCompletionCard();

        pressBack();
        onViewWaiting(withText(R.string.privacy_guide_cookies_intro));

        pressBack();
        onViewWaiting(withText(R.string.privacy_guide_safe_browsing_intro));

        pressBack();
        onViewWaiting(withText(R.string.privacy_guide_history_sync_toggle));

        pressBack();
        onViewWaiting(withText(R.string.url_keyed_anonymized_data_title));

        pressBack();
        onViewWaiting(withText(R.string.privacy_guide_fragment_title));
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testWelcomeCard_nextClickWelcomeUserAction() {
        launchPrivacyGuide();
        navigateFromWelcomeToMSBBCard();
        // Verify that the user action is emitted when the next button is clicked on the welcome
        // page
        assertTrue(mActionTester.getActions().contains("Settings.PrivacyGuide.NextClickWelcome"));
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testWelcomeCard_nextNavigationHistogram() {
        launchPrivacyGuide();

        var histogram = HistogramWatcher.newSingleRecordWatcher(
                NEXT_NAVIGATION_HISTOGRAM, PrivacyGuideInteractions.WELCOME_NEXT_BUTTON);

        navigateFromWelcomeToMSBBCard();

        histogram.assertExpected();
    }

    @Test
    @LargeTest
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
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testCompletionCard_nextNavigationHistogram() {
        launchPrivacyGuide();
        goToCompletionCard();

        var histogram = HistogramWatcher.newSingleRecordWatcher(
                NEXT_NAVIGATION_HISTOGRAM, PrivacyGuideInteractions.COMPLETION_NEXT_BUTTON);

        // Complete page -> EXIT
        onView(withText(R.string.done)).perform(click());

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    @Features.EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_3)
    @Features.DisableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4)
    public void testCompletionCard_PrivacySandboxLinkNavigation() {
        launchPrivacyGuide();
        goToCompletionCard();

        onViewWaiting(withId(R.id.ps_button)).perform(scrollTo(), click());
        onViewWaiting(withText(R.string.privacy_sandbox_trials_title))
                .check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    @Features.EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4)
    @Features.DisableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_3)
    public void testCompletionCard_AdPrivacyLinkNavigation() {
        launchPrivacyGuide();
        goToCompletionCard();

        onViewWaiting(withId(R.id.ps_button)).perform(scrollTo(), click());
        onViewWaiting(withText(R.string.ad_privacy_page_title)).check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testCompletionCard_AdPrivacyClickUserAction() {
        launchPrivacyGuide();
        goToCompletionCard();

        onViewWaiting(withId(R.id.ps_button)).perform(scrollTo(), click());
        assertTrue(mActionTester.getActions().contains("Settings.PrivacyGuide.CompletionPSClick"));
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testCompletionCard_AdPrivacyClickHistogram() {
        launchPrivacyGuide();
        goToCompletionCard();

        var histogram = HistogramWatcher.newSingleRecordWatcher(
                ENTRY_EXIT_HISTOGRAM, PrivacyGuideInteractions.PRIVACY_SANDBOX_COMPLETION_LINK);

        onViewWaiting(withId(R.id.ps_button)).perform(scrollTo(), click());

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testCompletionCard_WaaLinkNavigation() {
        launchPrivacyGuide();
        goToCompletionCard();

        executeWhileCapturingIntents(() -> {
            onViewWaiting(withId(R.id.waa_button)).perform(scrollTo(), click());
            intended(IntentMatchers.hasData(
                    UrlConstants.GOOGLE_ACCOUNT_ACTIVITY_CONTROLS_FROM_PG_URL));
        });
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testCompletionCard_WaaClickUserAction() {
        launchPrivacyGuide();
        goToCompletionCard();

        executeWhileCapturingIntents(
                () -> onViewWaiting(withId(R.id.waa_button)).perform(scrollTo(), click()));

        assertTrue(
                mActionTester.getActions().contains("Settings.PrivacyGuide.CompletionSWAAClick"));
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testCompletionCard_WaaClickHistogram() {
        launchPrivacyGuide();
        goToCompletionCard();

        var histogram = HistogramWatcher.newSingleRecordWatcher(
                ENTRY_EXIT_HISTOGRAM, PrivacyGuideInteractions.SWAA_COMPLETION_LINK);

        executeWhileCapturingIntents(
                () -> onViewWaiting(withId(R.id.waa_button)).perform(scrollTo(), click()));

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testMSBBCard_nextClickMSBBUserAction() {
        launchPrivacyGuide();
        goToHistorySyncCard();

        assertTrue(mActionTester.getActions().contains("Settings.PrivacyGuide.NextClickMSBB"));
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testMSBBCard_nextNavigationHistogram() {
        launchPrivacyGuide();
        navigateFromWelcomeToMSBBCard();

        var histogram = HistogramWatcher.newSingleRecordWatcher(
                NEXT_NAVIGATION_HISTOGRAM, PrivacyGuideInteractions.MSBB_NEXT_BUTTON);

        navigateFromMSBBToHistorySyncCard();

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testMSBBCard_offToOffSettingsStatesHistogram() {
        setMSBBState(false);
        launchPrivacyGuide();
        navigateFromWelcomeToMSBBCard();

        var histogram = HistogramWatcher.newSingleRecordWatcher(
                SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.MSBB_OFF_TO_OFF);

        navigateFromMSBBToHistorySyncCard();

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testMSBBCard_offToOnSettingsStatesHistogram() {
        setMSBBState(false);
        launchPrivacyGuide();
        navigateFromWelcomeToMSBBCard();

        var histogram = HistogramWatcher.newSingleRecordWatcher(
                SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.MSBB_OFF_TO_ON);

        onView(withId(R.id.msbb_switch)).perform(click());
        navigateFromMSBBToHistorySyncCard();

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testMSBBCard_onToOffSettingsStatesHistogram() {
        setMSBBState(true);
        launchPrivacyGuide();
        navigateFromWelcomeToMSBBCard();

        var histogram = HistogramWatcher.newSingleRecordWatcher(
                SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.MSBB_ON_TO_OFF);

        onView(withId(R.id.msbb_switch)).perform(click());
        navigateFromMSBBToHistorySyncCard();

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testMSBBCard_onToOnSettingsStatesHistogram() {
        setMSBBState(true);
        launchPrivacyGuide();
        navigateFromWelcomeToMSBBCard();

        var histogram = HistogramWatcher.newSingleRecordWatcher(
                SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.MSBB_ON_TO_ON);

        navigateFromMSBBToHistorySyncCard();

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testMSBBCard_nextButtonInitialMSBBStateIsSet() {
        launchPrivacyGuide();
        mPrivacyGuideTestRule.getFragment().setPrivacyGuideMetricsDelegateForTesting(
                mPrivacyGuideMetricsDelegateMock);
        navigateFromWelcomeToMSBBCard();

        verify(mPrivacyGuideMetricsDelegateMock)
                .setInitialStateForCard(PrivacyGuideFragment.FragmentType.MSBB);

        mPrivacyGuideTestRule.getFragment().setPrivacyGuideMetricsDelegateForTesting(null);
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testMSBBCard_nextButtonAfterActivityRecreation() {
        setMSBBState(false);
        launchPrivacyGuide();
        navigateFromWelcomeToMSBBCard();

        var histogram = HistogramWatcher.newSingleRecordWatcher(
                SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.MSBB_OFF_TO_ON);

        onView(withId(R.id.msbb_switch)).perform(click());
        mPrivacyGuideTestRule.recreateActivity();
        navigateFromMSBBToHistorySyncCard();

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testMSBBCard_backButtonInitialMSBBStateIsSet() {
        launchPrivacyGuide();
        mPrivacyGuideTestRule.getFragment().setPrivacyGuideMetricsDelegateForTesting(
                mPrivacyGuideMetricsDelegateMock);

        goToHistorySyncCard();
        navigateFromHistorySyncToMSBBCard();

        verify(mPrivacyGuideMetricsDelegateMock, times(2))
                .setInitialStateForCard(PrivacyGuideFragment.FragmentType.MSBB);

        mPrivacyGuideTestRule.getFragment().setPrivacyGuideMetricsDelegateForTesting(null);
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testHistorySyncCard_nextClickHistorySyncUserAction() {
        launchPrivacyGuide();
        mActionTester = new UserActionTester();
        goToSafeBrowsingCard();

        assertTrue(
                mActionTester.getActions().contains("Settings.PrivacyGuide.NextClickHistorySync"));
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testHistorySyncCard_nextNavigationHistogram() {
        launchPrivacyGuide();
        goToHistorySyncCard();

        var histogram = HistogramWatcher.newSingleRecordWatcher(
                NEXT_NAVIGATION_HISTOGRAM, PrivacyGuideInteractions.HISTORY_SYNC_NEXT_BUTTON);

        navigateFromHistorySyncToSBCard();

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testHistorySyncCard_offToOffSettingsStatesHistogram() {
        setHistorySyncState(false);
        launchPrivacyGuide();
        goToHistorySyncCard();

        var histogram = HistogramWatcher.newSingleRecordWatcher(
                SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.HISTORY_SYNC_OFF_TO_OFF);

        navigateFromHistorySyncToSBCard();

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testHistorySyncCard_offToOnSettingsStatesHistogram() {
        setHistorySyncState(false);
        launchPrivacyGuide();
        goToHistorySyncCard();

        var histogram = HistogramWatcher.newSingleRecordWatcher(
                SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.HISTORY_SYNC_OFF_TO_ON);

        onView(withId(R.id.history_sync_switch)).perform(click());
        navigateFromHistorySyncToSBCard();

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testHistorySyncCard_onToOffSettingsStatesHistogram() {
        setHistorySyncState(true);
        launchPrivacyGuide();
        goToHistorySyncCard();

        var histogram = HistogramWatcher.newSingleRecordWatcher(
                SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.HISTORY_SYNC_ON_TO_OFF);

        onView(withId(R.id.history_sync_switch)).perform(click());
        navigateFromHistorySyncToSBCard();

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testHistorySyncCard_onToOnSettingsStatesHistogram() {
        setHistorySyncState(true);
        launchPrivacyGuide();
        goToHistorySyncCard();

        var histogram = HistogramWatcher.newSingleRecordWatcher(
                SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.HISTORY_SYNC_ON_TO_ON);

        navigateFromHistorySyncToSBCard();

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testHistorySyncCard_nextButtonAfterActivityRecreation() {
        setHistorySyncState(false);
        launchPrivacyGuide();
        goToHistorySyncCard();

        var histogram = HistogramWatcher.newSingleRecordWatcher(
                SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.HISTORY_SYNC_OFF_TO_ON);

        onView(withId(R.id.history_sync_switch)).perform(click());
        mPrivacyGuideTestRule.recreateActivity();
        navigateFromHistorySyncToSBCard();

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testHistorySyncCard_nextButtonInitialSyncStateIsSet() {
        launchPrivacyGuide();
        mPrivacyGuideTestRule.getFragment().setPrivacyGuideMetricsDelegateForTesting(
                mPrivacyGuideMetricsDelegateMock);
        goToHistorySyncCard();

        verify(mPrivacyGuideMetricsDelegateMock)
                .setInitialStateForCard(PrivacyGuideFragment.FragmentType.HISTORY_SYNC);

        mPrivacyGuideTestRule.getFragment().setPrivacyGuideMetricsDelegateForTesting(null);
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testHistorySyncCard_backButtonInitialSyncStateIsSet() {
        launchPrivacyGuide();
        mPrivacyGuideTestRule.getFragment().setPrivacyGuideMetricsDelegateForTesting(
                mPrivacyGuideMetricsDelegateMock);

        goToSafeBrowsingCard();
        navigateFromSBToHistorySyncCard();

        verify(mPrivacyGuideMetricsDelegateMock, times(2))
                .setInitialStateForCard(PrivacyGuideFragment.FragmentType.HISTORY_SYNC);

        mPrivacyGuideTestRule.getFragment().setPrivacyGuideMetricsDelegateForTesting(null);
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testSafeBrowsingCard_nextClickSafeBrowsingUserAction() {
        launchPrivacyGuide();
        goToCookiesCard();

        assertTrue(
                mActionTester.getActions().contains("Settings.PrivacyGuide.NextClickSafeBrowsing"));
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testSafeBrowsingCard_nextNavigationHistogram() {
        launchPrivacyGuide();
        goToSafeBrowsingCard();

        var histogram = HistogramWatcher.newSingleRecordWatcher(
                NEXT_NAVIGATION_HISTOGRAM, PrivacyGuideInteractions.SAFE_BROWSING_NEXT_BUTTON);

        navigateFromSBToCookiesCard();

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testSafeBrowsingCard_standardToStandardSettingsStatesHistogram() {
        setSafeBrowsingState(SafeBrowsingState.STANDARD_PROTECTION);
        launchPrivacyGuide();
        goToSafeBrowsingCard();

        var histogram = HistogramWatcher.newSingleRecordWatcher(SETTINGS_STATES_HISTOGRAM,
                PrivacyGuideSettingsStates.SAFE_BROWSING_STANDARD_TO_STANDARD);

        navigateFromSBToCookiesCard();

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testSafeBrowsingCard_standardToEnhancedSettingsStatesHistogram() {
        setSafeBrowsingState(SafeBrowsingState.STANDARD_PROTECTION);
        launchPrivacyGuide();
        goToSafeBrowsingCard();

        var histogram = HistogramWatcher.newSingleRecordWatcher(SETTINGS_STATES_HISTOGRAM,
                PrivacyGuideSettingsStates.SAFE_BROWSING_STANDARD_TO_ENHANCED);

        onView(withId(R.id.enhanced_option)).perform(click());
        navigateFromSBToCookiesCard();

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testSafeBrowsingCard_enhancedToEnhancedSettingsStatesHistogram() {
        setSafeBrowsingState(SafeBrowsingState.ENHANCED_PROTECTION);
        launchPrivacyGuide();
        goToSafeBrowsingCard();

        var histogram = HistogramWatcher.newSingleRecordWatcher(SETTINGS_STATES_HISTOGRAM,
                PrivacyGuideSettingsStates.SAFE_BROWSING_ENHANCED_TO_ENHANCED);

        navigateFromSBToCookiesCard();

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testSafeBrowsingCard_enhancedToStandardSettingsStatesHistogram() {
        setSafeBrowsingState(SafeBrowsingState.ENHANCED_PROTECTION);
        launchPrivacyGuide();
        goToSafeBrowsingCard();

        var histogram = HistogramWatcher.newSingleRecordWatcher(SETTINGS_STATES_HISTOGRAM,
                PrivacyGuideSettingsStates.SAFE_BROWSING_ENHANCED_TO_STANDARD);

        onView(withId(R.id.standard_option)).perform(scrollTo(), click());
        navigateFromSBToCookiesCard();

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testSafeBrowsingCard_nextButtonAfterActivityRecreation() {
        setSafeBrowsingState(SafeBrowsingState.STANDARD_PROTECTION);
        launchPrivacyGuide();
        goToSafeBrowsingCard();

        var histogram = HistogramWatcher.newSingleRecordWatcher(SETTINGS_STATES_HISTOGRAM,
                PrivacyGuideSettingsStates.SAFE_BROWSING_STANDARD_TO_ENHANCED);

        onView(withId(R.id.enhanced_option)).perform(click());
        mPrivacyGuideTestRule.recreateActivity();
        navigateFromSBToCookiesCard();

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testSafeBrowsingCard_nextButtonInitialSafeBrowsingStateIsSet() {
        launchPrivacyGuide();
        mPrivacyGuideTestRule.getFragment().setPrivacyGuideMetricsDelegateForTesting(
                mPrivacyGuideMetricsDelegateMock);
        goToSafeBrowsingCard();

        verify(mPrivacyGuideMetricsDelegateMock)
                .setInitialStateForCard(PrivacyGuideFragment.FragmentType.SAFE_BROWSING);

        mPrivacyGuideTestRule.getFragment().setPrivacyGuideMetricsDelegateForTesting(null);
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testSafeBrowsingCard_backButtonInitialSafeBrowsingStateIsSet() {
        launchPrivacyGuide();
        mPrivacyGuideTestRule.getFragment().setPrivacyGuideMetricsDelegateForTesting(
                mPrivacyGuideMetricsDelegateMock);

        goToCookiesCard();
        navigateFromCookiesToSBCard();

        verify(mPrivacyGuideMetricsDelegateMock, times(2))
                .setInitialStateForCard(PrivacyGuideFragment.FragmentType.SAFE_BROWSING);

        mPrivacyGuideTestRule.getFragment().setPrivacyGuideMetricsDelegateForTesting(null);
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    @Features.EnableFeatures(ChromeFeatureList.PRIVACY_GUIDE_POST_MVP)
    public void testSafeBrowsingCard_enhancedBottomSheetBackButtonBehaviour() {
        launchPrivacyGuide();
        goToSafeBrowsingCard();

        clickOnArrowNextToRadioButtonWithText(R.string.privacy_guide_safe_browsing_enhanced_title);
        onViewWaiting(withId(R.id.sb_enhanced_sheet)).check(matches(isDisplayed()));

        pressBack();
        onViewWaiting(withText(R.string.privacy_guide_safe_browsing_enhanced_title))
                .check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    @Features.EnableFeatures(ChromeFeatureList.PRIVACY_GUIDE_POST_MVP)
    public void testSafeBrowsingCard_standardBottomSheetBackButtonBehaviour() {
        launchPrivacyGuide();
        goToSafeBrowsingCard();

        clickOnArrowNextToRadioButtonWithText(R.string.privacy_guide_safe_browsing_standard_title);
        onViewWaiting(withId(R.id.sb_standard_sheet)).check(matches(isDisplayed()));

        pressBack();
        onViewWaiting(withText(R.string.privacy_guide_safe_browsing_standard_title))
                .check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testCookiesCard_nextClickCookiesUserAction() {
        launchPrivacyGuide();
        goToCompletionCard();

        assertTrue(mActionTester.getActions().contains("Settings.PrivacyGuide.NextClickCookies"));
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testCookiesCard_nextNavigationHistogram() {
        launchPrivacyGuide();
        goToCookiesCard();

        var histogram = HistogramWatcher.newSingleRecordWatcher(
                NEXT_NAVIGATION_HISTOGRAM, PrivacyGuideInteractions.COOKIES_NEXT_BUTTON);

        navigateFromCookiesToCompletionCard();

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testCookiesCard_block3PIncognitoTo3PIncognitoSettingsStatesHistogram() {
        setCookieControlsMode(CookieControlsMode.INCOGNITO_ONLY);
        launchPrivacyGuide();
        goToCookiesCard();

        var histogram = HistogramWatcher.newSingleRecordWatcher(SETTINGS_STATES_HISTOGRAM,
                PrivacyGuideSettingsStates.BLOCK3P_INCOGNITO_TO3P_INCOGNITO);

        navigateFromCookiesToCompletionCard();

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testCookiesCard_block3PIncognitoTo3PSettingsStatesHistogram() {
        setCookieControlsMode(CookieControlsMode.INCOGNITO_ONLY);
        launchPrivacyGuide();
        goToCookiesCard();

        var histogram = HistogramWatcher.newSingleRecordWatcher(
                SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.BLOCK3P_INCOGNITO_TO3P);

        onView(withId(R.id.block_third_party)).perform(scrollTo(), click());
        navigateFromCookiesToCompletionCard();

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testCookiesCard_block3PTo3PIncognitoSettingsStatesHistogram() {
        setCookieControlsMode(CookieControlsMode.BLOCK_THIRD_PARTY);
        launchPrivacyGuide();
        goToCookiesCard();

        var histogram = HistogramWatcher.newSingleRecordWatcher(
                SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.BLOCK3P_TO3P_INCOGNITO);

        onView(withId(R.id.block_third_party_incognito)).perform(click());
        navigateFromCookiesToCompletionCard();

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testCookiesCard_block3PTo3PSettingsStatesHistogram() {
        setCookieControlsMode(CookieControlsMode.BLOCK_THIRD_PARTY);
        launchPrivacyGuide();
        goToCookiesCard();

        var histogram = HistogramWatcher.newSingleRecordWatcher(
                SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.BLOCK3P_TO3P);

        navigateFromCookiesToCompletionCard();

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testCookiesCard_nextButtonAfterActivityRecreation() {
        setCookieControlsMode(CookieControlsMode.INCOGNITO_ONLY);
        launchPrivacyGuide();
        goToCookiesCard();

        var histogram = HistogramWatcher.newSingleRecordWatcher(
                SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.BLOCK3P_INCOGNITO_TO3P);

        onView(withId(R.id.block_third_party)).perform(scrollTo(), click());
        mPrivacyGuideTestRule.recreateActivity();
        navigateFromCookiesToCompletionCard();

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testCookiesCard_nextButtonInitialCookiesStateIsSet() {
        launchPrivacyGuide();
        mPrivacyGuideTestRule.getFragment().setPrivacyGuideMetricsDelegateForTesting(
                mPrivacyGuideMetricsDelegateMock);
        goToCookiesCard();

        verify(mPrivacyGuideMetricsDelegateMock)
                .setInitialStateForCard(PrivacyGuideFragment.FragmentType.COOKIES);

        mPrivacyGuideTestRule.getFragment().setPrivacyGuideMetricsDelegateForTesting(null);
    }

    @Test
    @LargeTest
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
    @LargeTest
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
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testCookiesCard_backClickCookiesUserAction() {
        launchPrivacyGuide();

        goToCookiesCard();
        navigateFromCookiesToSBCard();

        // Verify that the user action is emitted when the back button is clicked on the Cookies
        // card
        assertTrue(mActionTester.getActions().contains("Settings.PrivacyGuide.BackClickCookies"));
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testBottomSheetControllerOnRecreate() {
        launchPrivacyGuide();
        goToSafeBrowsingCard();
        mPrivacyGuideTestRule.recreateActivity();
        clickOnArrowNextToRadioButtonWithText(R.string.privacy_guide_safe_browsing_enhanced_title);
        onViewWaiting(withId(R.id.sb_enhanced_sheet)).check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    @Features.EnableFeatures(ChromeFeatureList.PRIVACY_GUIDE)
    @Features.DisableFeatures(ChromeFeatureList.PRIVACY_GUIDE_POST_MVP)
    public void testExitOnBackInWelcomeCard() {
        launchPrivacySettingsAndOpenPrivacyGuide();

        // Exit the guide when pressing back.
        pressBack();
        onView(withText(R.string.privacy_guide_pref_summary)).check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    @Features.EnableFeatures(ChromeFeatureList.PRIVACY_GUIDE)
    @Features.DisableFeatures(ChromeFeatureList.PRIVACY_GUIDE_POST_MVP)
    public void testExitOnBackInMSBBCard() {
        launchPrivacySettingsAndOpenPrivacyGuide();
        navigateFromWelcomeToMSBBCard();

        // Exit the guide when pressing back.
        pressBack();
        onView(withText(R.string.privacy_guide_pref_summary)).check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    @Features.EnableFeatures(ChromeFeatureList.PRIVACY_GUIDE)
    @Features.DisableFeatures(ChromeFeatureList.PRIVACY_GUIDE_POST_MVP)
    public void testExitOnBackInHistorySyncCard() {
        launchPrivacySettingsAndOpenPrivacyGuide();
        goToHistorySyncCard();

        // Exit the guide when pressing back.
        pressBack();
        onView(withText(R.string.privacy_guide_pref_summary)).check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    @Features.EnableFeatures(ChromeFeatureList.PRIVACY_GUIDE)
    @Features.DisableFeatures(ChromeFeatureList.PRIVACY_GUIDE_POST_MVP)
    public void testExitOnBackInSBCard() {
        launchPrivacySettingsAndOpenPrivacyGuide();
        goToSafeBrowsingCard();

        // Exit the guide when pressing back.
        pressBack();
        onView(withText(R.string.privacy_guide_pref_summary)).check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    @Features.EnableFeatures(ChromeFeatureList.PRIVACY_GUIDE)
    @Features.DisableFeatures(ChromeFeatureList.PRIVACY_GUIDE_POST_MVP)
    public void testExitOnBackInCookiesCard() {
        launchPrivacySettingsAndOpenPrivacyGuide();
        goToCookiesCard();

        // Exit the guide when pressing back.
        pressBack();
        onView(withText(R.string.privacy_guide_pref_summary)).check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    @Features.EnableFeatures(ChromeFeatureList.PRIVACY_GUIDE)
    @Features.DisableFeatures(ChromeFeatureList.PRIVACY_GUIDE_POST_MVP)
    public void testExitOnBackInCompletionCard() {
        launchPrivacySettingsAndOpenPrivacyGuide();
        goToCompletionCard();

        // Exit the guide when pressing back.
        pressBack();
        onView(withText(R.string.privacy_guide_pref_summary)).check(matches(isDisplayed()));
    }
}
