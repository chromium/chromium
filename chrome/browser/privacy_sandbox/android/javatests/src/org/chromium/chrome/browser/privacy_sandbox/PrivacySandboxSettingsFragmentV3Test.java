// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.isChecked;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.containsString;
import static org.hamcrest.Matchers.hasItem;
import static org.hamcrest.Matchers.hasItems;
import static org.hamcrest.Matchers.not;
import static org.hamcrest.Matchers.startsWith;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxTestUtils.clickImageButtonNextToText;
import static org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxTestUtils.withTopic;

import android.os.Bundle;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matcher;
import org.junit.After;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.metrics.HistogramTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.PayloadCallbackHelper;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.ui.test.util.ViewUtils;

import java.io.IOException;
import java.util.List;
import java.util.concurrent.ExecutionException;

/**
 * Tests {@link PrivacySandboxSettingsFragment}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_3)
@Features.DisableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4)
public final class PrivacySandboxSettingsFragmentV3Test {
    private static final String REFERRER_HISTOGRAM =
            "Settings.PrivacySandbox.PrivacySandboxReferrer";

    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public SettingsActivityTestRule<PrivacySandboxSettingsFragmentV3> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(PrivacySandboxSettingsFragmentV3.class);

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.UI_SETTINGS_PRIVACY)
                    .build();

    @Rule
    public HistogramTestRule mHistogramTestRule = new HistogramTestRule();

    @Rule
    public JniMocker mocker = new JniMocker();

    private FakePrivacySandboxBridge mFakePrivacySandboxBridge;

    private UserActionTester mUserActionTester;

    @BeforeClass
    public static void beforeClass() {
        // Only needs to be loaded once and needs to be loaded before HistogramTestRule.
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
    }

    @Before
    public void setUp() {
        mFakePrivacySandboxBridge = new FakePrivacySandboxBridge();
        mocker.mock(PrivacySandboxBridgeJni.TEST_HOOKS, mFakePrivacySandboxBridge);

        mFakePrivacySandboxBridge.setCurrentTopTopics("Foo", "Bar");
        mFakePrivacySandboxBridge.setBlockedTopics("BlockedFoo", "BlockedBar");
        mFakePrivacySandboxBridge.setCurrentFledgeSites("example.com", "example2.com");
        mFakePrivacySandboxBridge.setBlockedFledgeSites("blocked.com", "blocked2.com");
    }

    @After
    public void tearDown() {
        if (mUserActionTester != null) {
            mUserActionTester.tearDown();
        }
    }

    private void openPrivacySandboxSettings() {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putInt(PrivacySandboxSettingsFragmentV3.PRIVACY_SANDBOX_REFERRER,
                PrivacySandboxReferrer.PRIVACY_SETTINGS);
        mSettingsActivityTestRule.startSettingsActivity(fragmentArgs);
        ViewUtils.onViewWaiting(withText(R.string.privacy_sandbox_trials_title));
    }

    private View getRootView(@StringRes int text) {
        View[] view = {null};
        onView(withText(text)).check(((v, e) -> view[0] = v.getRootView()));
        TestThreadUtils.runOnUiThreadBlocking(() -> RenderTestRule.sanitize(view[0]));
        return view[0];
    }

    private void scrollToSetting(Matcher<View> matcher) {
        onView(withId(R.id.recycler_view))
                .perform(RecyclerViewActions.scrollTo(hasDescendant(matcher)));
    }

    private boolean isPrivacySandboxEnabled() throws ExecutionException {
        return TestThreadUtils.runOnUiThreadBlocking(PrivacySandboxBridge::isPrivacySandboxEnabled);
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderMainPage() throws IOException {
        openPrivacySandboxSettings();
        mRenderTestRule.render(
                getRootView(R.string.privacy_sandbox_trials_title), "privacy_sandbox_main_view");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderAdPersonalizationView() throws IOException {
        mFakePrivacySandboxBridge.setCurrentTopTopics("Generated sample data", "More made up data");
        mFakePrivacySandboxBridge.setCurrentFledgeSites("a.com", "b.com");
        openPrivacySandboxSettings();
        onView(withText(R.string.privacy_sandbox_ad_personalization_title)).perform(click());
        mRenderTestRule.render(getRootView(R.string.privacy_sandbox_topic_interests_subtitle),
                "privacy_sandbox_ad_personalization_view");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderRemovedInterestsView() throws IOException {
        openPrivacySandboxSettings();
        onView(withText(R.string.privacy_sandbox_ad_personalization_title)).perform(click());
        onView(withText(R.string.privacy_sandbox_remove_interest_title)).perform(click());
        mRenderTestRule.render(getRootView(R.string.privacy_sandbox_topic_interests_subtitle),
                "privacy_sandbox_removed_interests_view");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderLearnMoreView() throws IOException {
        openPrivacySandboxSettings();
        onView(withText(containsString("About"))).perform(click());
        mRenderTestRule.render(getRootView(R.string.privacy_sandbox_learn_more_title),
                "privacy_sandbox_learn_more_view");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderAdMeasurementView() throws IOException {
        openPrivacySandboxSettings();
        onView(withText(R.string.privacy_sandbox_ad_measurement_title)).perform(click());
        onView(withText(containsString("Ad measurement allows sites")))
                .check(matches(isDisplayed()));
        mRenderTestRule.render(getRootView(R.string.privacy_sandbox_ad_measurement_title),
                "privacy_sandbox_ad_measurement_view");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderSpamFraudView() throws IOException {
        openPrivacySandboxSettings();
        scrollToSetting(withText(R.string.privacy_sandbox_spam_fraud_title));
        onView(withText(R.string.privacy_sandbox_spam_fraud_title)).perform(click());
        mRenderTestRule.render(
                getRootView(R.string.privacy_sandbox_spam_fraud_description_trials_on),
                "privacy_sandbox_spam_fraud_view");
    }

    @Test
    @SmallTest
    public void testMainSettingsView() throws IOException, ExecutionException {
        // Reset mock to test the real instance.
        mocker.mock(PrivacySandboxBridgeJni.TEST_HOOKS, null);
        Matcher<View> sandboxCheckboxMatcher = allOf(withId(R.id.switchWidget),
                withParent(withParent(
                        hasDescendant(withText(R.string.privacy_sandbox_trials_title)))));
        // Initially setting is off.
        openPrivacySandboxSettings();
        onView(sandboxCheckboxMatcher).check(matches(not(isChecked())));
        assertFalse("Disabled initially", isPrivacySandboxEnabled());

        // Toggle sandbox settings on.
        onView(withText(R.string.privacy_sandbox_trials_title)).perform(click());
        onView(sandboxCheckboxMatcher).check(matches(isChecked()));
        assertTrue("Then enabled", isPrivacySandboxEnabled());

        // Toggle sandbox settings off.
        onView(withText(R.string.privacy_sandbox_trials_title)).perform(click());
        onView(sandboxCheckboxMatcher).check(matches(not(isChecked())));
        assertFalse("And disabled again", isPrivacySandboxEnabled());
    }

    @Test
    @SmallTest
    public void testAdPersonalizationView() throws IOException {
        openPrivacySandboxSettings();
        onView(withText(R.string.privacy_sandbox_ad_personalization_title)).perform(click());
        onView(withText(R.string.privacy_sandbox_remove_interest_title))
                .check(matches(isDisplayed()));
        onView(withText(R.string.privacy_sandbox_ad_personalization_description_trials_on))
                .check(matches(isDisplayed()));
        onView(withText(R.string.privacy_sandbox_topic_empty_state)).check(doesNotExist());
        onView(withText(R.string.privacy_sandbox_fledge_empty_state)).check(doesNotExist());
    }

    @Test
    @SmallTest
    public void testAdPersonalizationTopics() throws IOException {
        openPrivacySandboxSettings();
        mUserActionTester = new UserActionTester();
        onView(withText(R.string.privacy_sandbox_ad_personalization_title)).perform(click());

        clickImageButtonNextToText("Foo");
        assertThat(PrivacySandboxBridge.getCurrentTopTopics(), not(hasItem(withTopic("Foo"))));
        assertThat(PrivacySandboxBridge.getBlockedTopics(), hasItem(withTopic("Foo")));
        onView(withText(R.string.privacy_sandbox_remove_interest_snackbar))
                .check(matches(isDisplayed()));
        assertThat(mUserActionTester.getActions(),
                hasItems("Settings.PrivacySandbox.AdPersonalization.Opened",
                        "Settings.PrivacySandbox.AdPersonalization.TopicRemoved"));
        onView(withText(R.string.privacy_sandbox_topic_empty_state)).check(doesNotExist());

        clickImageButtonNextToText("Bar");
        onView(withText(R.string.privacy_sandbox_topic_empty_state)).check(matches(isDisplayed()));
    }

    @Nullable
    private List<String> getFledgeSites() {
        PayloadCallbackHelper<List<String>> callbackHelper = new PayloadCallbackHelper<>();
        PrivacySandboxBridge.getFledgeJoiningEtldPlusOneForDisplay(callbackHelper::notifyCalled);
        return callbackHelper.getOnlyPayloadBlocking();
    }

    @Test
    @SmallTest
    public void testAdPersonalizationFledge() throws IOException {
        openPrivacySandboxSettings();
        mUserActionTester = new UserActionTester();
        onView(withText(R.string.privacy_sandbox_ad_personalization_title)).perform(click());

        scrollToSetting(withText("example.com"));
        clickImageButtonNextToText("example.com");
        assertThat(getFledgeSites(), not(hasItem("example.com")));
        assertThat(PrivacySandboxBridge.getBlockedFledgeJoiningTopFramesForDisplay(),
                hasItem("example.com"));
        onView(withText(R.string.privacy_sandbox_remove_site_snackbar))
                .check(matches(isDisplayed()));
        assertThat(mUserActionTester.getActions(),
                hasItems("Settings.PrivacySandbox.AdPersonalization.Opened",
                        "Settings.PrivacySandbox.AdPersonalization.SiteRemoved"));
        onView(withText(R.string.privacy_sandbox_fledge_empty_state)).check(doesNotExist());

        clickImageButtonNextToText("example2.com");
        onView(withText(R.string.privacy_sandbox_fledge_empty_state)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testAdPersonalizationEmptyView() throws IOException {
        // Set no current or blocked topics.
        mFakePrivacySandboxBridge.setCurrentTopTopics();
        mFakePrivacySandboxBridge.setBlockedTopics();
        mFakePrivacySandboxBridge.setCurrentFledgeSites();
        mFakePrivacySandboxBridge.setBlockedFledgeSites();
        openPrivacySandboxSettings();
        onView(withText(R.string.privacy_sandbox_ad_personalization_title)).perform(click());
        onView(withText(R.string.privacy_sandbox_ad_personalization_description_no_items))
                .check(matches(isDisplayed()));
        onView(withText(R.string.privacy_sandbox_remove_interest_title)).check(doesNotExist());
        onView(withText(R.string.privacy_sandbox_remove_sites_title)).check(doesNotExist());
        onView(withText(R.string.privacy_sandbox_topic_empty_state)).check(matches(isDisplayed()));
        onView(withText(R.string.privacy_sandbox_fledge_empty_state)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testAdPersonalizationEmptyViewTrialsOff() throws IOException {
        PrivacySandboxBridge.setPrivacySandboxEnabled(false);
        openPrivacySandboxSettings();
        onView(withText(R.string.privacy_sandbox_ad_personalization_title)).perform(click());
        onView(withText(R.string.privacy_sandbox_ad_personalization_description_trials_off))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testAdMeasurementView() {
        openPrivacySandboxSettings();
        onView(withText(R.string.privacy_sandbox_ad_measurement_title)).perform(click());
        onView(withText(startsWith("Ad measurement allows sites"))).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testAdMeasurementViewTrialsOff() {
        PrivacySandboxBridge.setPrivacySandboxEnabled(false);
        openPrivacySandboxSettings();
        onView(withText(R.string.privacy_sandbox_ad_measurement_title)).perform(click());
        onView(withText(startsWith("When trials are on, Ad measurement allows")))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testSpamFraudView() {
        openPrivacySandboxSettings();
        scrollToSetting(withText(R.string.privacy_sandbox_spam_fraud_title));
        onView(withText(R.string.privacy_sandbox_spam_fraud_title)).perform(click());
        onView(withText(R.string.privacy_sandbox_spam_fraud_description_trials_on))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testSpamFraudViewTrialsOff() {
        PrivacySandboxBridge.setPrivacySandboxEnabled(false);
        openPrivacySandboxSettings();
        scrollToSetting(withText(R.string.privacy_sandbox_spam_fraud_title));
        onView(withText(R.string.privacy_sandbox_spam_fraud_title)).perform(click());
        onView(withText(R.string.privacy_sandbox_spam_fraud_description_trials_off))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testRemovedInterestsViewForTopics() throws IOException {
        openPrivacySandboxSettings();
        mUserActionTester = new UserActionTester();
        onView(withText(R.string.privacy_sandbox_ad_personalization_title)).perform(click());
        onView(withText(R.string.privacy_sandbox_remove_interest_title)).perform(click());

        clickImageButtonNextToText("BlockedFoo");
        assertThat(PrivacySandboxBridge.getCurrentTopTopics(), hasItem(withTopic("BlockedFoo")));
        assertThat(PrivacySandboxBridge.getBlockedTopics(), not(hasItem(withTopic("BlockedFoo"))));
        onView(withText(R.string.privacy_sandbox_add_interest_snackbar))
                .check(matches(isDisplayed()));
        assertThat(mUserActionTester.getActions(),
                hasItems("Settings.PrivacySandbox.RemovedInterests.Opened",
                        "Settings.PrivacySandbox.RemovedInterests.TopicAdded"));
        onView(withText(R.string.privacy_sandbox_removed_topics_empty_state)).check(doesNotExist());

        clickImageButtonNextToText("BlockedBar");
        onView(withText(R.string.privacy_sandbox_removed_topics_empty_state))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testRemovedInterestsViewForFledge() throws IOException {
        openPrivacySandboxSettings();
        mUserActionTester = new UserActionTester();
        onView(withText(R.string.privacy_sandbox_ad_personalization_title)).perform(click());
        onView(withText(R.string.privacy_sandbox_remove_interest_title)).perform(click());

        scrollToSetting(withText("blocked2.com"));
        clickImageButtonNextToText("blocked.com");
        assertThat(getFledgeSites(), hasItem("blocked.com"));
        assertThat(PrivacySandboxBridge.getBlockedFledgeJoiningTopFramesForDisplay(),
                not(hasItem("blocked.com")));
        onView(withText(R.string.privacy_sandbox_add_site_snackbar)).check(matches(isDisplayed()));
        assertThat(mUserActionTester.getActions(),
                hasItems("Settings.PrivacySandbox.RemovedInterests.Opened",
                        "Settings.PrivacySandbox.RemovedInterests.SiteAdded"));
        onView(withText(R.string.privacy_sandbox_removed_sites_empty_state)).check(doesNotExist());

        clickImageButtonNextToText("blocked2.com");
        onView(withText(R.string.privacy_sandbox_removed_sites_empty_state))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testCreateActivityFromPrivacySettings() {
        openPrivacySandboxSettings();
        assertEquals("Total histogram count wrong", 1,
                mHistogramTestRule.getHistogramTotalCount(REFERRER_HISTOGRAM));
        assertEquals("Privacy referrer histogram count", 1,
                mHistogramTestRule.getHistogramValueCount(
                        REFERRER_HISTOGRAM, PrivacySandboxReferrer.PRIVACY_SETTINGS));
    }

    @Test
    @SmallTest
    public void testCreateActivityFromCookiesSnackbar() {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putInt(PrivacySandboxSettingsFragmentV3.PRIVACY_SANDBOX_REFERRER,
                PrivacySandboxReferrer.COOKIES_SNACKBAR);
        mSettingsActivityTestRule.startSettingsActivity(fragmentArgs);

        assertEquals("Total histogram count", 1,
                mHistogramTestRule.getHistogramTotalCount(REFERRER_HISTOGRAM));
        assertEquals("Cookies snackbar referrer histogram count wrong", 1,
                mHistogramTestRule.getHistogramValueCount(
                        REFERRER_HISTOGRAM, PrivacySandboxReferrer.COOKIES_SNACKBAR));
    }
}
