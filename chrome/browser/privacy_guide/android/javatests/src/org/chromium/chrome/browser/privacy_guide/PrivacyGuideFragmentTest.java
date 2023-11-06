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
import static androidx.test.espresso.matcher.ViewMatchers.isChecked;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
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
import android.os.Build.VERSION_CODES;
import android.view.View;

import androidx.test.espresso.ViewInteraction;
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
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.prefetch.settings.PreloadPagesSettingsBridge;
import org.chromium.chrome.browser.prefetch.settings.PreloadPagesState;
import org.chromium.chrome.browser.privacy.settings.PrivacySettings;
import org.chromium.chrome.browser.privacy_guide.PrivacyGuideFragment.FragmentType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridge;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.content_settings.CookieControlsMode;
import org.chromium.components.content_settings.PrefNames;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.components.user_prefs.UserPrefs;

import java.io.IOException;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** Tests {@link PrivacyGuideFragment} */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PrivacyGuideFragmentTest {
    private static final String SETTINGS_STATES_HISTOGRAM = "Settings.PrivacyGuide.SettingsStates";
    private static final String NEXT_NAVIGATION_HISTOGRAM = "Settings.PrivacyGuide.NextNavigation";
    private static final String ENTRY_EXIT_HISTOGRAM = "Settings.PrivacyGuide.EntryExit";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule public ChromeBrowserTestRule mChromeBrowserTestRule = new ChromeBrowserTestRule();

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

    @Mock private PrivacyGuideMetricsDelegate mPrivacyGuideMetricsDelegateMock;

    private UserActionTester mActionTester;

    private List<Integer> mAllFragments;

    @Before
    public void setUp() {
        if (ChromeFeatureList.sPrivacyGuideAndroid3.isEnabled()
                && ChromeFeatureList.sPrivacyGuidePreloadAndroid.isEnabled()) {
            mAllFragments = PrivacyGuideFragment.ALL_FRAGMENT_TYPE_ORDER_PG3;
        } else {
            mAllFragments = PrivacyGuideFragment.ALL_FRAGMENT_TYPE_ORDER;
        }
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

    private Map<Integer, Integer> mTitleNames =
            Map.of(
                    FragmentType.MSBB,
                    R.string.url_keyed_anonymized_data_title,
                    FragmentType.HISTORY_SYNC,
                    R.string.privacy_guide_history_sync_toggle,
                    FragmentType.COOKIES,
                    R.string.privacy_guide_cookies_intro,
                    FragmentType.SAFE_BROWSING,
                    R.string.privacy_guide_safe_browsing_intro,
                    FragmentType.SEARCH_SUGGESTIONS,
                    R.string.improve_search_suggestions_title,
                    FragmentType.PRELOAD,
                    R.string.preload_pages_privacy_guide_summary,
                    FragmentType.DONE,
                    R.string.privacy_guide_done_title);

    private @FragmentType int getNextCardType(@FragmentType int cardType) {
        int nextCardPosition = mAllFragments.indexOf(cardType) + 1;
        assertTrue("This is the last card in the flow.", nextCardPosition < mAllFragments.size());
        return mAllFragments.get(nextCardPosition);
    }

    private @FragmentType int getPreviousCardType(@FragmentType int cardType) {
        int previousCardPosition = mAllFragments.indexOf(cardType) - 1;
        assertTrue("This is the first card in the flow.", previousCardPosition >= 0);
        return mAllFragments.get(previousCardPosition);
    }

    private void navigateFromCardToNext(@FragmentType int cardType) {
        int numberOfMaxSteps = mAllFragments.size();
        int cardPosition = mAllFragments.indexOf(cardType);
        assertTrue(cardPosition < numberOfMaxSteps - 1);
        if (cardPosition == 0) {
            onView(withText(R.string.privacy_guide_start_button)).perform(click());
        } else if (cardPosition == numberOfMaxSteps - 2) {
            onView(withText(R.string.privacy_guide_finish_button)).perform(click());
        } else {
            onView(withText(R.string.next)).perform(click());
        }
        @FragmentType int nextCardType = getNextCardType(cardType);
        onViewWaiting(withText(mTitleNames.get(nextCardType)));
    }

    private void navigateFromCardToPrevious(@FragmentType int cardType) {
        int cardPosition = mAllFragments.indexOf(cardType);
        assertTrue(cardPosition > 0);
        onView(withText(R.string.back)).perform(click());
        @FragmentType int previousCardType = getPreviousCardType(cardType);
        onViewWaiting(withText(mTitleNames.get(previousCardType)));
    }

    private void goToCard(@FragmentType int cardType) {
        assertTrue(cardType != FragmentType.WELCOME);
        if (cardType == FragmentType.MSBB) {
            navigateFromCardToNext(FragmentType.WELCOME);
            return;
        }
        int previousCardType = getPreviousCardType(cardType);
        goToCard(previousCardType);
        navigateFromCardToNext(previousCardType);
    }

    private void setMSBBState(boolean isMSBBon) {
        runOnUiThreadBlocking(
                () ->
                        UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(
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
                () ->
                        UserPrefs.get(Profile.getLastUsedRegularProfile())
                                .setInteger(PrefNames.COOKIE_CONTROLS_MODE, cookieControlsMode));
    }

    private void setSearchSuggestionsStatePG3(boolean isSearchSuggestionsOn) {
        runOnUiThreadBlocking(
                () ->
                        UserPrefs.get(Profile.getLastUsedRegularProfile())
                                .setBoolean(Pref.SEARCH_SUGGEST_ENABLED, isSearchSuggestionsOn));
    }

    private void setPreloadStatePG3(@PreloadPagesState int preloadPagesState) {
        runOnUiThreadBlocking(() -> PreloadPagesSettingsBridge.setState(preloadPagesState));
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

    private void testButtonVisibility(
            boolean nextVisible, boolean backVisible, boolean finishVisible) {
        testButtonVisibility(R.string.next, nextVisible);
        testButtonVisibility(R.string.back, backVisible);
        testButtonVisibility(R.string.privacy_guide_finish_button, finishVisible);
    }

    private View getRootView() {
        return mPrivacyGuideTestRule.getActivity().findViewById(android.R.id.content).getRootView();
    }

    private void clickOnArrowNextToRadioButtonWithText(int textId) {
        onView(
                        allOf(
                                withId(R.id.expand_arrow),
                                withParent(hasSibling(withChild(withText(textId))))))
                .perform(scrollTo(), click());
    }

    private ViewInteraction onInternalRadioButtonOfViewWithId(int viewId) {
        return onView(allOf(withId(R.id.radio_button), isDescendantOfA(withId(viewId))));
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
    @DisableFeatures(ChromeFeatureList.PRIVACY_GUIDE_ANDROID_3)
    public void testRenderMSBBCard() throws IOException {
        launchPrivacyGuide();
        goToCard(FragmentType.MSBB);
        mRenderTestRule.render(getRootView(), "privacy_guide_msbb");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    @EnableFeatures(ChromeFeatureList.PRIVACY_GUIDE_ANDROID_3)
    public void testRenderMSBBCardPG3() throws IOException {
        launchPrivacyGuide();
        goToCard(FragmentType.MSBB);
        mRenderTestRule.render(getRootView(), "privacy_guide_msbb_v3");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testRenderHistorySyncCard() throws IOException {
        launchPrivacyGuide();
        goToCard(FragmentType.HISTORY_SYNC);
        mRenderTestRule.render(getRootView(), "privacy_guide_history_sync");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testRenderSBCard() throws IOException {
        launchPrivacyGuide();
        goToCard(FragmentType.SAFE_BROWSING);
        mRenderTestRule.render(getRootView(), "privacy_guide_sb");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testRenderSBEnhancedBottomSheet() throws IOException {
        launchPrivacyGuide();
        goToCard(FragmentType.SAFE_BROWSING);
        clickOnArrowNextToRadioButtonWithText(R.string.privacy_guide_safe_browsing_enhanced_title);
        mRenderTestRule.render(getRootView(), "privacy_guide_sb_enhanced_sheet");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    @EnableFeatures(ChromeFeatureList.FRIENDLIER_SAFE_BROWSING_SETTINGS_ENHANCED_PROTECTION)
    public void testRenderSBFriendlierEnhancedBottomSheet() throws IOException {
        launchPrivacyGuide();
        goToCard(FragmentType.SAFE_BROWSING);
        clickOnArrowNextToRadioButtonWithText(R.string.privacy_guide_safe_browsing_enhanced_title);
        mRenderTestRule.render(getRootView(), "privacy_guide_sb_enhanced_sheet_friendlier");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testRenderSBStandardBottomSheet() throws IOException {
        launchPrivacyGuide();
        goToCard(FragmentType.SAFE_BROWSING);
        clickOnArrowNextToRadioButtonWithText(R.string.privacy_guide_safe_browsing_standard_title);
        mRenderTestRule.render(getRootView(), "privacy_guide_sb_standard_sheet");
    }

    // TODO(crbug.com/1466292): Remove once friendlier safe browsing settings standard protection is
    // launched.
    @Test
    @LargeTest
    @Feature({"HashPrefixRealTimeLookupsTest"})
    @DisableFeatures({
        ChromeFeatureList.FRIENDLIER_SAFE_BROWSING_SETTINGS_STANDARD_PROTECTION,
        ChromeFeatureList.HASH_PREFIX_REAL_TIME_LOOKUPS
    })
    public void testRenderSBStandardBottomSheetTextWithoutProxy() throws IOException {
        launchPrivacyGuide();
        goToCard(FragmentType.SAFE_BROWSING);
        clickOnArrowNextToRadioButtonWithText(R.string.privacy_guide_safe_browsing_standard_title);
        onViewWaiting(withText(R.string.privacy_guide_sb_standard_item_two))
                .check(matches(isDisplayed()));
        onViewWaiting(withText(R.string.privacy_guide_sb_standard_item_three))
                .check(matches(isDisplayed()));
    }

    // TODO(crbug.com/1466292): Remove once friendlier safe browsing settings standard protection is
    // launched.
    @Test
    @LargeTest
    @Feature({"HashPrefixRealTimeLookupsTest"})
    @EnableFeatures(ChromeFeatureList.HASH_PREFIX_REAL_TIME_LOOKUPS)
    @DisableFeatures(ChromeFeatureList.FRIENDLIER_SAFE_BROWSING_SETTINGS_STANDARD_PROTECTION)
    public void testRenderSBStandardBottomSheetTextWithProxy() throws IOException {
        launchPrivacyGuide();
        goToCard(FragmentType.SAFE_BROWSING);
        clickOnArrowNextToRadioButtonWithText(R.string.privacy_guide_safe_browsing_standard_title);
        if (BuildConfig.IS_CHROME_BRANDED) {
            onViewWaiting(withText(R.string.privacy_guide_sb_standard_item_two_proxy))
                    .check(matches(isDisplayed()));
            onViewWaiting(withText(R.string.privacy_guide_sb_standard_item_three_proxy))
                    .check(matches(isDisplayed()));
        } else {
            // hash-prefix real-time check is disabled on Chromium build.
            onViewWaiting(withText(R.string.privacy_guide_sb_standard_item_two))
                    .check(matches(isDisplayed()));
            onViewWaiting(withText(R.string.privacy_guide_sb_standard_item_three))
                    .check(matches(isDisplayed()));
        }
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testRenderCookiesCard() throws IOException {
        launchPrivacyGuide();
        goToCard(FragmentType.COOKIES);
        mRenderTestRule.render(getRootView(), "privacy_guide_cookies");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    @EnableFeatures({
        ChromeFeatureList.PRIVACY_GUIDE_ANDROID_3,
        ChromeFeatureList.PRIVACY_GUIDE_PRELOAD_ANDROID
    })
    public void testRenderSearchSuggestionsCardPG3() throws IOException {
        launchPrivacyGuide();
        goToCard(FragmentType.SEARCH_SUGGESTIONS);
        mRenderTestRule.render(getRootView(), "privacy_guide_search_suggestions");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    @EnableFeatures({
        ChromeFeatureList.PRIVACY_GUIDE_ANDROID_3,
        ChromeFeatureList.PRIVACY_GUIDE_PRELOAD_ANDROID
    })
    public void testRenderPreloadCardPG3() throws IOException {
        launchPrivacyGuide();
        goToCard(FragmentType.PRELOAD);
        mRenderTestRule.render(getRootView(), "privacy_guide_preload");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    @EnableFeatures({
        ChromeFeatureList.PRIVACY_GUIDE_ANDROID_3,
        ChromeFeatureList.PRIVACY_GUIDE_PRELOAD_ANDROID
    })
    public void testRenderPreloadStandardBottomSheetPG3() throws IOException {
        launchPrivacyGuide();
        goToCard(FragmentType.PRELOAD);
        clickOnArrowNextToRadioButtonWithText(R.string.preload_pages_standard_preloading_title);
        mRenderTestRule.render(getRootView(), "privacy_guide_preload_standard_sheet");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testRenderCompletionCard() throws IOException {
        launchPrivacyGuide();
        goToCard(FragmentType.DONE);
        mRenderTestRule.render(getRootView(), "privacy_guide_completion");
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    @EnableFeatures(ChromeFeatureList.PRIVACY_GUIDE_POST_MVP)
    public void testForwardNavAllActions() {
        setMSBBState(false);
        setHistorySyncState(false);
        setSafeBrowsingState(SafeBrowsingState.STANDARD_PROTECTION);
        setCookieControlsMode(CookieControlsMode.INCOGNITO_ONLY);

        launchPrivacyGuide();
        testButtonVisibility(false, false, false);

        navigateFromCardToNext(FragmentType.WELCOME);
        testButtonVisibility(true, true, false);
        onView(withId(R.id.msbb_switch)).perform(click());
        onView(withId(R.id.msbb_switch)).check(matches(isChecked()));

        navigateFromCardToNext(FragmentType.MSBB);
        testButtonVisibility(true, true, false);
        onView(withId(R.id.history_sync_switch)).perform(click());
        onView(withId(R.id.history_sync_switch)).check(matches(isChecked()));

        navigateFromCardToNext(FragmentType.HISTORY_SYNC);
        testButtonVisibility(true, true, false);
        onView(withId(R.id.enhanced_option)).perform(click());
        onInternalRadioButtonOfViewWithId(R.id.enhanced_option).check(matches(isChecked()));

        navigateFromCardToNext(FragmentType.SAFE_BROWSING);
        testButtonVisibility(false, true, true);
        onView(withId(R.id.block_third_party)).perform(click());
        onInternalRadioButtonOfViewWithId(R.id.block_third_party).check(matches(isChecked()));

        navigateFromCardToNext(FragmentType.COOKIES);
        testButtonVisibility(false, false, false);
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    @EnableFeatures({
        ChromeFeatureList.PRIVACY_GUIDE_POST_MVP,
        ChromeFeatureList.PRIVACY_GUIDE_ANDROID_3,
        ChromeFeatureList.PRIVACY_GUIDE_PRELOAD_ANDROID
    })
    public void testForwardNavAllActionsPreloadPG3() {
        setMSBBState(false);
        setHistorySyncState(false);
        setSearchSuggestionsStatePG3(false);
        setPreloadStatePG3(PreloadPagesState.STANDARD_PRELOADING);
        setSafeBrowsingState(SafeBrowsingState.STANDARD_PROTECTION);
        setCookieControlsMode(CookieControlsMode.INCOGNITO_ONLY);

        launchPrivacyGuide();
        testButtonVisibility(false, false, false);

        navigateFromCardToNext(FragmentType.WELCOME);
        testButtonVisibility(true, true, false);
        onView(withId(R.id.msbb_switch)).perform(click());
        onView(withId(R.id.msbb_switch)).check(matches(isChecked()));

        navigateFromCardToNext(FragmentType.MSBB);
        testButtonVisibility(true, true, false);
        onView(withId(R.id.history_sync_switch)).perform(click());
        onView(withId(R.id.history_sync_switch)).check(matches(isChecked()));

        navigateFromCardToNext(FragmentType.HISTORY_SYNC);
        testButtonVisibility(true, true, false);
        onView(withId(R.id.block_third_party)).perform(click());
        onInternalRadioButtonOfViewWithId(R.id.block_third_party).check(matches(isChecked()));

        navigateFromCardToNext(FragmentType.COOKIES);
        testButtonVisibility(true, true, false);
        onView(withId(R.id.enhanced_option)).perform(click());
        onInternalRadioButtonOfViewWithId(R.id.enhanced_option).check(matches(isChecked()));

        navigateFromCardToNext(FragmentType.SAFE_BROWSING);
        testButtonVisibility(true, true, false);
        onView(withId(R.id.search_suggestions_switch)).perform(click());
        onView(withId(R.id.search_suggestions_switch)).check(matches(isChecked()));

        navigateFromCardToNext(FragmentType.SEARCH_SUGGESTIONS);
        testButtonVisibility(false, true, true);
        onView(withId(R.id.standard_option)).perform(click());
        onInternalRadioButtonOfViewWithId(R.id.standard_option).check(matches(isChecked()));

        navigateFromCardToNext(FragmentType.PRELOAD);
        testButtonVisibility(false, false, false);
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    @EnableFeatures(ChromeFeatureList.PRIVACY_GUIDE_POST_MVP)
    public void testBackwardNavAllActions() {
        setMSBBState(false);
        setHistorySyncState(false);
        setSafeBrowsingState(SafeBrowsingState.STANDARD_PROTECTION);
        setCookieControlsMode(CookieControlsMode.INCOGNITO_ONLY);

        launchPrivacyGuide();
        goToCard(FragmentType.DONE);

        pressBack();
        onViewWaiting(withText(R.string.privacy_guide_cookies_intro));
        onViewWaiting(allOf(withId(R.id.block_third_party), isCompletelyDisplayed()));
        onInternalRadioButtonOfViewWithId(R.id.block_third_party).perform(click());
        onInternalRadioButtonOfViewWithId(R.id.block_third_party).check(matches(isChecked()));

        pressBack();
        onViewWaiting(withText(R.string.privacy_guide_safe_browsing_intro));
        onViewWaiting(allOf(withId(R.id.enhanced_option), isCompletelyDisplayed()));
        onInternalRadioButtonOfViewWithId(R.id.enhanced_option).perform(click());
        onInternalRadioButtonOfViewWithId(R.id.enhanced_option).check(matches(isChecked()));

        pressBack();
        onViewWaiting(allOf(withId(R.id.history_sync_switch), isCompletelyDisplayed()));
        onView(withId(R.id.history_sync_switch)).perform(click());
        onView(withId(R.id.history_sync_switch)).check(matches(isChecked()));

        pressBack();
        onViewWaiting(allOf(withId(R.id.msbb_switch), isCompletelyDisplayed()));
        onView(withId(R.id.msbb_switch)).perform(click());
        onView(withId(R.id.msbb_switch)).check(matches(isChecked()));

        pressBack();
        onViewWaiting(withText(R.string.privacy_guide_fragment_title));
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    @EnableFeatures({
        ChromeFeatureList.PRIVACY_GUIDE_POST_MVP,
        ChromeFeatureList.PRIVACY_GUIDE_ANDROID_3,
        ChromeFeatureList.PRIVACY_GUIDE_PRELOAD_ANDROID
    })
    @DisableIf.Build(
            message = "Flaky on P. See http://crbug.com/1487153",
            sdk_is_greater_than = VERSION_CODES.O,
            sdk_is_less_than = VERSION_CODES.Q)
    public void testBackwardNavAllActionsPreloadPG3() {
        setMSBBState(false);
        setHistorySyncState(false);
        setSearchSuggestionsStatePG3(false);
        setPreloadStatePG3(PreloadPagesState.STANDARD_PRELOADING);
        setSafeBrowsingState(SafeBrowsingState.STANDARD_PROTECTION);
        setCookieControlsMode(CookieControlsMode.INCOGNITO_ONLY);

        launchPrivacyGuide();
        goToCard(FragmentType.DONE);

        pressBack();
        onViewWaiting(withText(R.string.preload_pages_privacy_guide_summary));
        onViewWaiting(allOf(withId(R.id.standard_option), isCompletelyDisplayed()));
        onInternalRadioButtonOfViewWithId(R.id.standard_option).perform(click());
        onInternalRadioButtonOfViewWithId(R.id.standard_option).check(matches(isChecked()));

        pressBack();
        onViewWaiting(allOf(withId(R.id.search_suggestions_switch), isCompletelyDisplayed()));
        onView(withId(R.id.search_suggestions_switch)).perform(click());
        onView(withId(R.id.search_suggestions_switch)).check(matches(isChecked()));

        pressBack();
        onViewWaiting(withText(R.string.privacy_guide_safe_browsing_intro));
        onViewWaiting(allOf(withId(R.id.enhanced_option), isCompletelyDisplayed()));
        onInternalRadioButtonOfViewWithId(R.id.enhanced_option).perform(click());
        onInternalRadioButtonOfViewWithId(R.id.enhanced_option).check(matches(isChecked()));

        pressBack();
        onViewWaiting(withText(R.string.privacy_guide_cookies_intro));
        onViewWaiting(allOf(withId(R.id.block_third_party), isCompletelyDisplayed()));
        onInternalRadioButtonOfViewWithId(R.id.block_third_party).perform(click());
        onInternalRadioButtonOfViewWithId(R.id.block_third_party).check(matches(isChecked()));

        pressBack();
        onViewWaiting(allOf(withId(R.id.history_sync_switch), isCompletelyDisplayed()));
        onView(withId(R.id.history_sync_switch)).perform(click());
        onView(withId(R.id.history_sync_switch)).check(matches(isChecked()));

        pressBack();
        onViewWaiting(allOf(withId(R.id.msbb_switch), isCompletelyDisplayed()));
        onView(withId(R.id.msbb_switch)).perform(click());
        onView(withId(R.id.msbb_switch)).check(matches(isChecked()));

        pressBack();
        onViewWaiting(withText(R.string.privacy_guide_fragment_title));
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testWelcomeCard_nextClickWelcomeUserAction() {
        launchPrivacyGuide();
        goToCard(FragmentType.MSBB);
        // Verify that the user action is emitted when the next button is clicked on the welcome
        // page
        assertTrue(mActionTester.getActions().contains("Settings.PrivacyGuide.NextClickWelcome"));
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testWelcomeCard_nextNavigationHistogram() {
        launchPrivacyGuide();

        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        NEXT_NAVIGATION_HISTOGRAM, PrivacyGuideInteractions.WELCOME_NEXT_BUTTON);

        goToCard(FragmentType.MSBB);

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testCompletionCard_nextClickCompletionUserAction() {
        launchPrivacyGuide();
        goToCard(FragmentType.DONE);

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
        goToCard(FragmentType.DONE);

        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        NEXT_NAVIGATION_HISTOGRAM, PrivacyGuideInteractions.COMPLETION_NEXT_BUTTON);

        // Complete page -> EXIT
        onView(withText(R.string.done)).perform(click());

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testCompletionCard_AdPrivacyLinkNavigation() {
        launchPrivacyGuide();
        goToCard(FragmentType.DONE);

        onViewWaiting(withId(R.id.ps_button)).perform(scrollTo(), click());
        onViewWaiting(withText(R.string.ad_privacy_page_title)).check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testCompletionCard_AdPrivacyClickUserAction() {
        launchPrivacyGuide();
        goToCard(FragmentType.DONE);

        onViewWaiting(withId(R.id.ps_button)).perform(scrollTo(), click());
        assertTrue(mActionTester.getActions().contains("Settings.PrivacyGuide.CompletionPSClick"));
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testCompletionCard_AdPrivacyClickHistogram() {
        launchPrivacyGuide();
        goToCard(FragmentType.DONE);

        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        ENTRY_EXIT_HISTOGRAM,
                        PrivacyGuideInteractions.PRIVACY_SANDBOX_COMPLETION_LINK);

        onViewWaiting(withId(R.id.ps_button)).perform(scrollTo(), click());

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testCompletionCard_WaaLinkNavigation() {
        launchPrivacyGuide();
        goToCard(FragmentType.DONE);

        executeWhileCapturingIntents(
                () -> {
                    onViewWaiting(withId(R.id.waa_button)).perform(scrollTo(), click());
                    intended(
                            IntentMatchers.hasData(
                                    UrlConstants.GOOGLE_ACCOUNT_ACTIVITY_CONTROLS_FROM_PG_URL));
                });
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testCompletionCard_WaaClickUserAction() {
        launchPrivacyGuide();
        goToCard(FragmentType.DONE);

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
        goToCard(FragmentType.DONE);

        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
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
        goToCard(FragmentType.HISTORY_SYNC);

        assertTrue(mActionTester.getActions().contains("Settings.PrivacyGuide.NextClickMSBB"));
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testMSBBCard_nextNavigationHistogram() {
        launchPrivacyGuide();
        goToCard(FragmentType.MSBB);

        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        NEXT_NAVIGATION_HISTOGRAM, PrivacyGuideInteractions.MSBB_NEXT_BUTTON);

        navigateFromCardToNext(FragmentType.MSBB);

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testMSBBCard_offToOffSettingsStatesHistogram() {
        setMSBBState(false);
        launchPrivacyGuide();
        goToCard(FragmentType.MSBB);

        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.MSBB_OFF_TO_OFF);

        navigateFromCardToNext(FragmentType.MSBB);

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testMSBBCard_offToOnSettingsStatesHistogram() {
        setMSBBState(false);
        launchPrivacyGuide();
        goToCard(FragmentType.MSBB);

        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.MSBB_OFF_TO_ON);

        onView(withId(R.id.msbb_switch)).perform(click());
        navigateFromCardToNext(FragmentType.MSBB);

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testMSBBCard_onToOffSettingsStatesHistogram() {
        setMSBBState(true);
        launchPrivacyGuide();
        goToCard(FragmentType.MSBB);

        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.MSBB_ON_TO_OFF);

        onView(withId(R.id.msbb_switch)).perform(click());
        navigateFromCardToNext(FragmentType.MSBB);

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testMSBBCard_onToOnSettingsStatesHistogram() {
        setMSBBState(true);
        launchPrivacyGuide();
        goToCard(FragmentType.MSBB);

        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.MSBB_ON_TO_ON);

        navigateFromCardToNext(FragmentType.MSBB);

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testMSBBCard_nextButtonInitialMSBBStateIsSet() {
        launchPrivacyGuide();
        mPrivacyGuideTestRule
                .getFragment()
                .setPrivacyGuideMetricsDelegateForTesting(mPrivacyGuideMetricsDelegateMock);
        goToCard(FragmentType.MSBB);

        verify(mPrivacyGuideMetricsDelegateMock).setInitialStateForCard(FragmentType.MSBB);

        mPrivacyGuideTestRule.getFragment().setPrivacyGuideMetricsDelegateForTesting(null);
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testMSBBCard_nextButtonAfterActivityRecreation() {
        setMSBBState(false);
        launchPrivacyGuide();
        goToCard(FragmentType.MSBB);

        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.MSBB_OFF_TO_ON);

        onView(withId(R.id.msbb_switch)).perform(click());
        mPrivacyGuideTestRule.recreateActivity();
        navigateFromCardToNext(FragmentType.MSBB);

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testMSBBCard_backButtonInitialMSBBStateIsSet() {
        launchPrivacyGuide();
        mPrivacyGuideTestRule
                .getFragment()
                .setPrivacyGuideMetricsDelegateForTesting(mPrivacyGuideMetricsDelegateMock);

        goToCard(FragmentType.HISTORY_SYNC);
        navigateFromCardToPrevious(FragmentType.HISTORY_SYNC);

        verify(mPrivacyGuideMetricsDelegateMock, times(2))
                .setInitialStateForCard(FragmentType.MSBB);

        mPrivacyGuideTestRule.getFragment().setPrivacyGuideMetricsDelegateForTesting(null);
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testHistorySyncCard_nextClickHistorySyncUserAction() {
        launchPrivacyGuide();
        mActionTester = new UserActionTester();
        goToCard(FragmentType.SAFE_BROWSING);

        assertTrue(
                mActionTester.getActions().contains("Settings.PrivacyGuide.NextClickHistorySync"));
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testHistorySyncCard_nextNavigationHistogram() {
        launchPrivacyGuide();
        goToCard(FragmentType.HISTORY_SYNC);

        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        NEXT_NAVIGATION_HISTOGRAM,
                        PrivacyGuideInteractions.HISTORY_SYNC_NEXT_BUTTON);

        navigateFromCardToNext(FragmentType.HISTORY_SYNC);

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testHistorySyncCard_offToOffSettingsStatesHistogram() {
        setHistorySyncState(false);
        launchPrivacyGuide();
        goToCard(FragmentType.HISTORY_SYNC);

        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.HISTORY_SYNC_OFF_TO_OFF);

        navigateFromCardToNext(FragmentType.HISTORY_SYNC);

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testHistorySyncCard_offToOnSettingsStatesHistogram() {
        setHistorySyncState(false);
        launchPrivacyGuide();
        goToCard(FragmentType.HISTORY_SYNC);

        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.HISTORY_SYNC_OFF_TO_ON);

        onView(withId(R.id.history_sync_switch)).perform(click());
        navigateFromCardToNext(FragmentType.HISTORY_SYNC);

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testHistorySyncCard_onToOffSettingsStatesHistogram() {
        setHistorySyncState(true);
        launchPrivacyGuide();
        goToCard(FragmentType.HISTORY_SYNC);

        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.HISTORY_SYNC_ON_TO_OFF);

        onView(withId(R.id.history_sync_switch)).perform(click());
        navigateFromCardToNext(FragmentType.HISTORY_SYNC);

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testHistorySyncCard_onToOnSettingsStatesHistogram() {
        setHistorySyncState(true);
        launchPrivacyGuide();
        goToCard(FragmentType.HISTORY_SYNC);

        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.HISTORY_SYNC_ON_TO_ON);

        navigateFromCardToNext(FragmentType.HISTORY_SYNC);

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testHistorySyncCard_nextButtonAfterActivityRecreation() {
        setHistorySyncState(false);
        launchPrivacyGuide();
        goToCard(FragmentType.HISTORY_SYNC);

        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.HISTORY_SYNC_OFF_TO_ON);

        onView(withId(R.id.history_sync_switch)).perform(click());
        mPrivacyGuideTestRule.recreateActivity();
        navigateFromCardToNext(FragmentType.HISTORY_SYNC);

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testHistorySyncCard_nextButtonInitialSyncStateIsSet() {
        launchPrivacyGuide();
        mPrivacyGuideTestRule
                .getFragment()
                .setPrivacyGuideMetricsDelegateForTesting(mPrivacyGuideMetricsDelegateMock);
        goToCard(FragmentType.HISTORY_SYNC);

        verify(mPrivacyGuideMetricsDelegateMock).setInitialStateForCard(FragmentType.HISTORY_SYNC);

        mPrivacyGuideTestRule.getFragment().setPrivacyGuideMetricsDelegateForTesting(null);
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testHistorySyncCard_backButtonInitialSyncStateIsSet() {
        launchPrivacyGuide();
        mPrivacyGuideTestRule
                .getFragment()
                .setPrivacyGuideMetricsDelegateForTesting(mPrivacyGuideMetricsDelegateMock);

        goToCard(FragmentType.SAFE_BROWSING);
        navigateFromCardToPrevious(FragmentType.SAFE_BROWSING);

        verify(mPrivacyGuideMetricsDelegateMock, times(2))
                .setInitialStateForCard(FragmentType.HISTORY_SYNC);

        mPrivacyGuideTestRule.getFragment().setPrivacyGuideMetricsDelegateForTesting(null);
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testSafeBrowsingCard_nextClickSafeBrowsingUserAction() {
        launchPrivacyGuide();
        goToCard(FragmentType.COOKIES);

        assertTrue(
                mActionTester.getActions().contains("Settings.PrivacyGuide.NextClickSafeBrowsing"));
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testSafeBrowsingCard_nextNavigationHistogram() {
        launchPrivacyGuide();
        goToCard(FragmentType.SAFE_BROWSING);

        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        NEXT_NAVIGATION_HISTOGRAM,
                        PrivacyGuideInteractions.SAFE_BROWSING_NEXT_BUTTON);

        navigateFromCardToNext(FragmentType.SAFE_BROWSING);

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testSafeBrowsingCard_standardToStandardSettingsStatesHistogram() {
        setSafeBrowsingState(SafeBrowsingState.STANDARD_PROTECTION);
        launchPrivacyGuide();
        goToCard(FragmentType.SAFE_BROWSING);

        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.SAFE_BROWSING_STANDARD_TO_STANDARD);

        navigateFromCardToNext(FragmentType.SAFE_BROWSING);

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testSafeBrowsingCard_standardToEnhancedSettingsStatesHistogram() {
        setSafeBrowsingState(SafeBrowsingState.STANDARD_PROTECTION);
        launchPrivacyGuide();
        goToCard(FragmentType.SAFE_BROWSING);

        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.SAFE_BROWSING_STANDARD_TO_ENHANCED);

        onView(withId(R.id.enhanced_option)).perform(click());
        navigateFromCardToNext(FragmentType.SAFE_BROWSING);

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testSafeBrowsingCard_enhancedToEnhancedSettingsStatesHistogram() {
        setSafeBrowsingState(SafeBrowsingState.ENHANCED_PROTECTION);
        launchPrivacyGuide();
        goToCard(FragmentType.SAFE_BROWSING);

        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.SAFE_BROWSING_ENHANCED_TO_ENHANCED);

        navigateFromCardToNext(FragmentType.SAFE_BROWSING);

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testSafeBrowsingCard_enhancedToStandardSettingsStatesHistogram() {
        setSafeBrowsingState(SafeBrowsingState.ENHANCED_PROTECTION);
        launchPrivacyGuide();
        goToCard(FragmentType.SAFE_BROWSING);

        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.SAFE_BROWSING_ENHANCED_TO_STANDARD);

        onView(withId(R.id.standard_option)).perform(scrollTo(), click());
        navigateFromCardToNext(FragmentType.SAFE_BROWSING);

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testSafeBrowsingCard_nextButtonAfterActivityRecreation() {
        setSafeBrowsingState(SafeBrowsingState.STANDARD_PROTECTION);
        launchPrivacyGuide();
        goToCard(FragmentType.SAFE_BROWSING);

        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.SAFE_BROWSING_STANDARD_TO_ENHANCED);

        onView(withId(R.id.enhanced_option)).perform(click());
        mPrivacyGuideTestRule.recreateActivity();
        navigateFromCardToNext(FragmentType.SAFE_BROWSING);

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testSafeBrowsingCard_nextButtonInitialSafeBrowsingStateIsSet() {
        launchPrivacyGuide();
        mPrivacyGuideTestRule
                .getFragment()
                .setPrivacyGuideMetricsDelegateForTesting(mPrivacyGuideMetricsDelegateMock);
        goToCard(FragmentType.SAFE_BROWSING);

        verify(mPrivacyGuideMetricsDelegateMock).setInitialStateForCard(FragmentType.SAFE_BROWSING);

        mPrivacyGuideTestRule.getFragment().setPrivacyGuideMetricsDelegateForTesting(null);
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testSafeBrowsingCard_backButtonInitialSafeBrowsingStateIsSet() {
        launchPrivacyGuide();
        mPrivacyGuideTestRule
                .getFragment()
                .setPrivacyGuideMetricsDelegateForTesting(mPrivacyGuideMetricsDelegateMock);

        goToCard(FragmentType.COOKIES);
        navigateFromCardToPrevious(FragmentType.COOKIES);

        verify(mPrivacyGuideMetricsDelegateMock, times(2))
                .setInitialStateForCard(FragmentType.SAFE_BROWSING);

        mPrivacyGuideTestRule.getFragment().setPrivacyGuideMetricsDelegateForTesting(null);
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    @DisableFeatures(ChromeFeatureList.FRIENDLIER_SAFE_BROWSING_SETTINGS_ENHANCED_PROTECTION)
    @EnableFeatures(ChromeFeatureList.PRIVACY_GUIDE_POST_MVP)
    public void testSafeBrowsingCard_enhancedBottomSheetBackButtonBehaviour() {
        launchPrivacyGuide();
        goToCard(FragmentType.SAFE_BROWSING);

        clickOnArrowNextToRadioButtonWithText(R.string.privacy_guide_safe_browsing_enhanced_title);
        onViewWaiting(withId(R.id.sb_enhanced_sheet)).check(matches(isDisplayed()));

        pressBack();
        onViewWaiting(withText(R.string.privacy_guide_safe_browsing_enhanced_title))
                .check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    @EnableFeatures({
        ChromeFeatureList.PRIVACY_GUIDE_POST_MVP,
        ChromeFeatureList.FRIENDLIER_SAFE_BROWSING_SETTINGS_ENHANCED_PROTECTION
    })
    public void testSafeBrowsingCard_enhancedFriendlierBottomSheetBackButtonBehaviour() {
        launchPrivacyGuide();
        goToCard(FragmentType.SAFE_BROWSING);

        clickOnArrowNextToRadioButtonWithText(R.string.privacy_guide_safe_browsing_enhanced_title);
        onViewWaiting(withId(R.id.sb_enhanced_sheet_updated)).check(matches(isDisplayed()));

        pressBack();
        onViewWaiting(withText(R.string.privacy_guide_safe_browsing_enhanced_title))
                .check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    @EnableFeatures(ChromeFeatureList.PRIVACY_GUIDE_POST_MVP)
    public void testSafeBrowsingCard_standardBottomSheetBackButtonBehaviour() {
        launchPrivacyGuide();
        goToCard(FragmentType.SAFE_BROWSING);

        clickOnArrowNextToRadioButtonWithText(R.string.privacy_guide_safe_browsing_standard_title);
        onViewWaiting(withId(R.id.sb_standard_sheet)).check(matches(isDisplayed()));

        pressBack();
        onViewWaiting(
                allOf(
                        withText(R.string.privacy_guide_safe_browsing_standard_title),
                        isDisplayed()));
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testCookiesCard_nextClickCookiesUserAction() {
        launchPrivacyGuide();
        goToCard(FragmentType.DONE);

        assertTrue(mActionTester.getActions().contains("Settings.PrivacyGuide.NextClickCookies"));
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testCookiesCard_nextNavigationHistogram() {
        launchPrivacyGuide();
        goToCard(FragmentType.COOKIES);

        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        NEXT_NAVIGATION_HISTOGRAM, PrivacyGuideInteractions.COOKIES_NEXT_BUTTON);

        navigateFromCardToNext(FragmentType.COOKIES);

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testCookiesCard_block3PIncognitoTo3PIncognitoSettingsStatesHistogram() {
        setCookieControlsMode(CookieControlsMode.INCOGNITO_ONLY);
        launchPrivacyGuide();
        goToCard(FragmentType.COOKIES);

        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.BLOCK3P_INCOGNITO_TO3P_INCOGNITO);

        navigateFromCardToNext(FragmentType.COOKIES);

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testCookiesCard_block3PIncognitoTo3PSettingsStatesHistogram() {
        setCookieControlsMode(CookieControlsMode.INCOGNITO_ONLY);
        launchPrivacyGuide();
        goToCard(FragmentType.COOKIES);

        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.BLOCK3P_INCOGNITO_TO3P);

        onView(withId(R.id.block_third_party)).perform(scrollTo(), click());
        navigateFromCardToNext(FragmentType.COOKIES);

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testCookiesCard_block3PTo3PIncognitoSettingsStatesHistogram() {
        setCookieControlsMode(CookieControlsMode.BLOCK_THIRD_PARTY);
        launchPrivacyGuide();
        goToCard(FragmentType.COOKIES);

        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.BLOCK3P_TO3P_INCOGNITO);

        onView(withId(R.id.block_third_party_incognito)).perform(click());
        navigateFromCardToNext(FragmentType.COOKIES);

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testCookiesCard_block3PTo3PSettingsStatesHistogram() {
        setCookieControlsMode(CookieControlsMode.BLOCK_THIRD_PARTY);
        launchPrivacyGuide();
        goToCard(FragmentType.COOKIES);

        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.BLOCK3P_TO3P);

        navigateFromCardToNext(FragmentType.COOKIES);

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testCookiesCard_nextButtonAfterActivityRecreation() {
        setCookieControlsMode(CookieControlsMode.INCOGNITO_ONLY);
        launchPrivacyGuide();
        goToCard(FragmentType.COOKIES);

        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.BLOCK3P_INCOGNITO_TO3P);

        onView(withId(R.id.block_third_party)).perform(scrollTo(), click());
        mPrivacyGuideTestRule.recreateActivity();
        navigateFromCardToNext(FragmentType.COOKIES);

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testCookiesCard_nextButtonInitialCookiesStateIsSet() {
        launchPrivacyGuide();
        mPrivacyGuideTestRule
                .getFragment()
                .setPrivacyGuideMetricsDelegateForTesting(mPrivacyGuideMetricsDelegateMock);
        goToCard(FragmentType.COOKIES);

        verify(mPrivacyGuideMetricsDelegateMock).setInitialStateForCard(FragmentType.COOKIES);

        mPrivacyGuideTestRule.getFragment().setPrivacyGuideMetricsDelegateForTesting(null);
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    @EnableFeatures({
        ChromeFeatureList.PRIVACY_GUIDE_ANDROID_3,
        ChromeFeatureList.PRIVACY_GUIDE_PRELOAD_ANDROID
    })
    public void testCookiesCard_backButtonInitialCookiesStateIsSetPG3() {
        launchPrivacyGuide();
        mPrivacyGuideTestRule
                .getFragment()
                .setPrivacyGuideMetricsDelegateForTesting(mPrivacyGuideMetricsDelegateMock);

        goToCard(FragmentType.SAFE_BROWSING);
        navigateFromCardToPrevious(FragmentType.SAFE_BROWSING);

        verify(mPrivacyGuideMetricsDelegateMock, times(2))
                .setInitialStateForCard(FragmentType.COOKIES);

        mPrivacyGuideTestRule.getFragment().setPrivacyGuideMetricsDelegateForTesting(null);
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    @EnableFeatures({
        ChromeFeatureList.PRIVACY_GUIDE_ANDROID_3,
        ChromeFeatureList.PRIVACY_GUIDE_PRELOAD_ANDROID
    })
    public void testSearchSuggestionsCard_nextClickSearchSuggestionsUserActionPG3() {
        launchPrivacyGuide();
        goToCard(FragmentType.PRELOAD);

        assertTrue(
                mActionTester
                        .getActions()
                        .contains("Settings.PrivacyGuide.NextClickSearchSuggestions"));
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    @EnableFeatures({
        ChromeFeatureList.PRIVACY_GUIDE_ANDROID_3,
        ChromeFeatureList.PRIVACY_GUIDE_PRELOAD_ANDROID
    })
    public void testSearchSuggestionsCard_nextNavigationHistogramPG3() {
        launchPrivacyGuide();
        goToCard(FragmentType.SEARCH_SUGGESTIONS);

        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        NEXT_NAVIGATION_HISTOGRAM,
                        PrivacyGuideInteractions.SEARCH_SUGGESTIONS_NEXT_BUTTON);

        navigateFromCardToNext(FragmentType.SEARCH_SUGGESTIONS);

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    @EnableFeatures({
        ChromeFeatureList.PRIVACY_GUIDE_ANDROID_3,
        ChromeFeatureList.PRIVACY_GUIDE_PRELOAD_ANDROID
    })
    public void testSearchSuggestionsCard_offToOffSettingsStatesHistogramPG3() {
        setSearchSuggestionsStatePG3(false);
        launchPrivacyGuide();
        goToCard(FragmentType.SEARCH_SUGGESTIONS);

        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.SEARCH_SUGGESTIONS_OFF_TO_OFF);

        navigateFromCardToNext(FragmentType.SEARCH_SUGGESTIONS);

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    @EnableFeatures({
        ChromeFeatureList.PRIVACY_GUIDE_ANDROID_3,
        ChromeFeatureList.PRIVACY_GUIDE_PRELOAD_ANDROID
    })
    public void testSearchSuggestionsCard_offToOnSettingsStatesHistogramPG3() {
        setSearchSuggestionsStatePG3(false);
        launchPrivacyGuide();
        goToCard(FragmentType.SEARCH_SUGGESTIONS);

        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.SEARCH_SUGGESTIONS_OFF_TO_ON);

        onView(withId(R.id.search_suggestions_switch)).perform(click());
        navigateFromCardToNext(FragmentType.SEARCH_SUGGESTIONS);

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    @EnableFeatures({
        ChromeFeatureList.PRIVACY_GUIDE_ANDROID_3,
        ChromeFeatureList.PRIVACY_GUIDE_PRELOAD_ANDROID
    })
    public void testSearchSuggestionsCard_onToOffSettingsStatesHistogramPG3() {
        setSearchSuggestionsStatePG3(true);
        launchPrivacyGuide();
        goToCard(FragmentType.SEARCH_SUGGESTIONS);

        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.SEARCH_SUGGESTIONS_ON_TO_OFF);

        onView(withId(R.id.search_suggestions_switch)).perform(click());
        navigateFromCardToNext(FragmentType.SEARCH_SUGGESTIONS);

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    @EnableFeatures({
        ChromeFeatureList.PRIVACY_GUIDE_ANDROID_3,
        ChromeFeatureList.PRIVACY_GUIDE_PRELOAD_ANDROID
    })
    public void testSearchSuggestionsCard_onToOnSettingsStatesHistogramPG3() {
        setSearchSuggestionsStatePG3(true);
        launchPrivacyGuide();
        goToCard(FragmentType.SEARCH_SUGGESTIONS);

        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.SEARCH_SUGGESTIONS_ON_TO_ON);

        navigateFromCardToNext(FragmentType.SEARCH_SUGGESTIONS);

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    @EnableFeatures({
        ChromeFeatureList.PRIVACY_GUIDE_ANDROID_3,
        ChromeFeatureList.PRIVACY_GUIDE_PRELOAD_ANDROID
    })
    public void testSearchSuggestionsCard_nextButtonInitialSearchSuggestionsStateIsSetPG3() {
        launchPrivacyGuide();
        mPrivacyGuideTestRule
                .getFragment()
                .setPrivacyGuideMetricsDelegateForTesting(mPrivacyGuideMetricsDelegateMock);
        goToCard(FragmentType.SEARCH_SUGGESTIONS);

        verify(mPrivacyGuideMetricsDelegateMock)
                .setInitialStateForCard(FragmentType.SEARCH_SUGGESTIONS);

        mPrivacyGuideTestRule.getFragment().setPrivacyGuideMetricsDelegateForTesting(null);
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    @EnableFeatures({
        ChromeFeatureList.PRIVACY_GUIDE_ANDROID_3,
        ChromeFeatureList.PRIVACY_GUIDE_PRELOAD_ANDROID
    })
    public void testSearchSuggestionsCard_nextButtonAfterActivityRecreationPG3() {
        setSearchSuggestionsStatePG3(false);
        launchPrivacyGuide();
        goToCard(FragmentType.SEARCH_SUGGESTIONS);

        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.SEARCH_SUGGESTIONS_OFF_TO_ON);

        onView(withId(R.id.search_suggestions_switch)).perform(click());
        mPrivacyGuideTestRule.recreateActivity();
        navigateFromCardToNext(FragmentType.SEARCH_SUGGESTIONS);

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    public void testHistorySyncCard_backClickHistorySyncUserAction() {
        launchPrivacyGuide();

        goToCard(FragmentType.HISTORY_SYNC);
        navigateFromCardToPrevious(FragmentType.HISTORY_SYNC);

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

        goToCard(FragmentType.SAFE_BROWSING);
        navigateFromCardToPrevious(FragmentType.SAFE_BROWSING);

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

        goToCard(FragmentType.COOKIES);
        navigateFromCardToPrevious(FragmentType.COOKIES);

        // Verify that the user action is emitted when the back button is clicked on the Cookies
        // card
        assertTrue(mActionTester.getActions().contains("Settings.PrivacyGuide.BackClickCookies"));
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    @EnableFeatures({
        ChromeFeatureList.PRIVACY_GUIDE_ANDROID_3,
        ChromeFeatureList.PRIVACY_GUIDE_PRELOAD_ANDROID
    })
    public void testSearchSuggestionsCard_backClickSearchSuggestionsUserActionPG3() {
        launchPrivacyGuide();

        goToCard(FragmentType.SEARCH_SUGGESTIONS);
        navigateFromCardToPrevious(FragmentType.SEARCH_SUGGESTIONS);

        // Verify that the user action is emitted when the back button is clicked on the search
        // suggestions card
        assertTrue(
                mActionTester
                        .getActions()
                        .contains("Settings.PrivacyGuide.BackClickSearchSuggestions"));
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    @EnableFeatures({
        ChromeFeatureList.PRIVACY_GUIDE_POST_MVP,
        ChromeFeatureList.PRIVACY_GUIDE_ANDROID_3,
        ChromeFeatureList.PRIVACY_GUIDE_PRELOAD_ANDROID
    })
    public void testPreloadCardPG3_standardBottomSheetBackButtonBehaviour() {
        setPreloadStatePG3(PreloadPagesState.STANDARD_PRELOADING);
        launchPrivacyGuide();
        goToCard(FragmentType.PRELOAD);

        clickOnArrowNextToRadioButtonWithText(R.string.preload_pages_standard_preloading_title);
        onViewWaiting(withId(R.id.preload_standard_sheet)).check(matches(isDisplayed()));

        pressBack();
        onViewWaiting(withText(R.string.preload_pages_standard_preloading_title))
                .check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    @DisableFeatures(ChromeFeatureList.FRIENDLIER_SAFE_BROWSING_SETTINGS_ENHANCED_PROTECTION)
    public void testBottomSheetControllerOnRecreateOriginal() {
        launchPrivacyGuide();
        goToCard(FragmentType.SAFE_BROWSING);
        mPrivacyGuideTestRule.recreateActivity();
        clickOnArrowNextToRadioButtonWithText(R.string.privacy_guide_safe_browsing_enhanced_title);
        onViewWaiting(withId(R.id.sb_enhanced_sheet)).check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    @Feature({"PrivacyGuide"})
    @EnableFeatures(ChromeFeatureList.FRIENDLIER_SAFE_BROWSING_SETTINGS_ENHANCED_PROTECTION)
    public void testBottomSheetControllerOnRecreate() {
        launchPrivacyGuide();
        goToCard(FragmentType.SAFE_BROWSING);
        mPrivacyGuideTestRule.recreateActivity();
        clickOnArrowNextToRadioButtonWithText(R.string.privacy_guide_safe_browsing_enhanced_title);
        onViewWaiting(withId(R.id.sb_enhanced_sheet_updated)).check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    @EnableFeatures(ChromeFeatureList.PRIVACY_GUIDE)
    @DisableFeatures(ChromeFeatureList.PRIVACY_GUIDE_POST_MVP)
    public void testExitOnBackInWelcomeCard() {
        launchPrivacySettingsAndOpenPrivacyGuide();

        // Exit the guide when pressing back.
        pressBack();
        onView(withText(R.string.privacy_guide_pref_summary)).check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    @EnableFeatures(ChromeFeatureList.PRIVACY_GUIDE)
    @DisableFeatures(ChromeFeatureList.PRIVACY_GUIDE_POST_MVP)
    public void testExitOnBackInMSBBCard() {
        launchPrivacySettingsAndOpenPrivacyGuide();
        goToCard(FragmentType.MSBB);

        // Exit the guide when pressing back.
        pressBack();
        onView(withText(R.string.privacy_guide_pref_summary)).check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    @EnableFeatures(ChromeFeatureList.PRIVACY_GUIDE)
    @DisableFeatures(ChromeFeatureList.PRIVACY_GUIDE_POST_MVP)
    public void testExitOnBackInHistorySyncCard() {
        launchPrivacySettingsAndOpenPrivacyGuide();
        goToCard(FragmentType.HISTORY_SYNC);

        // Exit the guide when pressing back.
        pressBack();
        onView(withText(R.string.privacy_guide_pref_summary)).check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    @EnableFeatures(ChromeFeatureList.PRIVACY_GUIDE)
    @DisableFeatures(ChromeFeatureList.PRIVACY_GUIDE_POST_MVP)
    public void testExitOnBackInSBCard() {
        launchPrivacySettingsAndOpenPrivacyGuide();
        goToCard(FragmentType.SAFE_BROWSING);

        // Exit the guide when pressing back.
        pressBack();
        onView(withText(R.string.privacy_guide_pref_summary)).check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    @EnableFeatures(ChromeFeatureList.PRIVACY_GUIDE)
    @DisableFeatures(ChromeFeatureList.PRIVACY_GUIDE_POST_MVP)
    public void testExitOnBackInCookiesCard() {
        launchPrivacySettingsAndOpenPrivacyGuide();
        goToCard(FragmentType.COOKIES);

        // Exit the guide when pressing back.
        pressBack();
        onView(withText(R.string.privacy_guide_pref_summary)).check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    @EnableFeatures(ChromeFeatureList.PRIVACY_GUIDE)
    @DisableFeatures(ChromeFeatureList.PRIVACY_GUIDE_POST_MVP)
    public void testExitOnBackInCompletionCard() {
        launchPrivacySettingsAndOpenPrivacyGuide();
        goToCard(FragmentType.DONE);

        // Exit the guide when pressing back.
        pressBack();
        onView(withText(R.string.privacy_guide_pref_summary)).check(matches(isDisplayed()));
    }
}
