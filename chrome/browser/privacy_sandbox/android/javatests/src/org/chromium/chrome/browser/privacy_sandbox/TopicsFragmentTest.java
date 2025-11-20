// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.Espresso.pressBack;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.assertThat;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.isChecked;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.containsString;
import static org.hamcrest.Matchers.hasItems;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxTestUtils.clickImageButtonNextToText;
import static org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxTestUtils.clickRecyclerViewItemWithText;
import static org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxTestUtils.getRootViewSanitized;
import static org.chromium.ui.test.util.ViewUtils.clickOnClickableSpan;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.view.View;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matcher;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.ui.test.util.ViewUtils;

import java.io.IOException;

/** Tests {@link TopicsFragment} */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DisableFeatures(ChromeFeatureList.SETTINGS_MULTI_COLUMN)
public final class TopicsFragmentTest {
    private static final String TOPIC_NAME_1 = "Topic 1";
    private static final String TOPIC_NAME_2 = "Topic 2";
    private static final int RENDER_TEST_REVISION = 3;
    private String mBlockedTopicsHeadingText;

    @Rule public ChromeBrowserTestRule mChromeBrowserTestRule = new ChromeBrowserTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_PRIVACY_SANDBOX)
                    .setRevision(RENDER_TEST_REVISION)
                    .setDescription("Launched Ads API UX Enhancements")
                    .build();

    @Rule
    public SettingsActivityTestRule<TopicsFragment> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(TopicsFragment.class);

    private FakePrivacySandboxBridge mFakePrivacySandboxBridge;
    private UserActionTester mUserActionTester;

    @Before
    public void setUp() {
        mFakePrivacySandboxBridge = new FakePrivacySandboxBridge();
        PrivacySandboxBridgeJni.setInstanceForTesting(mFakePrivacySandboxBridge);

        mUserActionTester = new UserActionTester();
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PrefService prefService =
                            UserPrefs.get(ProfileManager.getLastUsedRegularProfile());
                    prefService.clearPref(Pref.PRIVACY_SANDBOX_M1_TOPICS_ENABLED);
                });

        mUserActionTester.tearDown();
    }

    private void startTopicsSettings() {
        mSettingsActivityTestRule.startSettingsActivity();
        mBlockedTopicsHeadingText =
                mSettingsActivityTestRule
                        .getActivity()
                        .getResources()
                        .getString(R.string.settings_topics_page_blocked_topics_heading_new);
        onViewWaiting(
                allOf(
                        withText(R.string.settings_topics_page_title),
                        withParent(withId(R.id.action_bar))));
    }

    private Matcher<View> getTopicsToggleMatcher() {
        return allOf(
                withId(R.id.switchWidget),
                withParent(
                        withParent(
                                hasDescendant(
                                        withText(R.string.settings_topics_page_toggle_label)))));
    }

    private View getTopicsRootView() {
        return getRootViewSanitized(R.string.settings_topics_page_toggle_sub_label_v2);
    }

    private View getTopicsRootViewAdTopicsContentParity() {
        return getRootViewSanitized(R.string.settings_ad_topics_page_toggle_sub_label);
    }

    private View getBlockedTopicsRootView() {
        return getRootViewSanitized(R.string.settings_topics_page_blocked_topics_heading_new);
    }

    private void setTopicsPrefEnabled(boolean isEnabled) {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        TopicsFragment.setTopicsPrefEnabled(
                                ProfileManager.getLastUsedRegularProfile(), isEnabled));
    }

    private boolean isTopicsPrefEnabled() {
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        TopicsFragment.isTopicsPrefEnabled(
                                ProfileManager.getLastUsedRegularProfile()));
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @DisableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_AD_TOPICS_CONTENT_PARITY)
    public void testRenderTopicsOff() throws IOException {
        setTopicsPrefEnabled(false);
        startTopicsSettings();
        mRenderTestRule.render(getTopicsRootView(), "topics_page_off");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @DisableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_AD_TOPICS_CONTENT_PARITY)
    public void testRenderTopicsEmpty() throws IOException {
        setTopicsPrefEnabled(true);
        startTopicsSettings();
        mRenderTestRule.render(getTopicsRootView(), "topics_page_empty");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @DisableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_AD_TOPICS_CONTENT_PARITY)
    public void testRenderTopicsPopulated() throws IOException {
        setTopicsPrefEnabled(true);
        mFakePrivacySandboxBridge.setCurrentTopTopics(TOPIC_NAME_1, TOPIC_NAME_2);
        startTopicsSettings();
        mRenderTestRule.render(getTopicsRootView(), "topic_page_populated");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderBlockedTopicsEmpty() throws IOException {
        setTopicsPrefEnabled(true);
        startTopicsSettings();
        clickRecyclerViewItemWithText(mBlockedTopicsHeadingText);
        mRenderTestRule.render(getBlockedTopicsRootView(), "blocked_topics_page_empty");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderBlockedTopicsPopulated() throws IOException {
        setTopicsPrefEnabled(true);
        mFakePrivacySandboxBridge.setBlockedTopics(TOPIC_NAME_1, TOPIC_NAME_2);
        startTopicsSettings();
        clickRecyclerViewItemWithText(mBlockedTopicsHeadingText);
        mRenderTestRule.render(getBlockedTopicsRootView(), "blocked_topics_page_populated");
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_AD_TOPICS_CONTENT_PARITY)
    public void adTopicsDisclaimerMetrics() throws IOException {
        setTopicsPrefEnabled(true);
        startTopicsSettings();
        String disclaimerText =
                mSettingsActivityTestRule
                        .getActivity()
                        .getResources()
                        .getString(R.string.settings_ad_topics_page_disclaimer_clank);
        String matcherText = disclaimerText.replaceAll("<link>|</link>", "");
        ViewUtils.onViewWaiting(withId(R.id.recycler_view))
                .perform(RecyclerViewActions.scrollTo(hasDescendant(withText(matcherText))));
        onView(withText(matcherText)).check(matches(isDisplayed()));
        onView(withText(matcherText)).perform(clickOnClickableSpan(0));
        assertEquals(
                1,
                mUserActionTester.getActionCount(
                        "Settings.PrivacySandbox.AdTopics.PrivacyPolicyLinkClicked"));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_AD_TOPICS_CONTENT_PARITY)
    public void adTopicsDisclaimerMetricsAdTopicsContentParity() throws IOException {
        setTopicsPrefEnabled(true);
        startTopicsSettings();
        String disclaimerText =
                mSettingsActivityTestRule
                        .getActivity()
                        .getResources()
                        .getString(R.string.settings_ad_topics_page_disclaimer_v2_clank);
        String matcherText = disclaimerText.replaceAll("<link>|</link>", "");
        ViewUtils.onViewWaiting(withId(R.id.recycler_view))
                .perform(RecyclerViewActions.scrollTo(hasDescendant(withText(matcherText))));
        onView(withText(matcherText)).check(matches(isDisplayed()));
        onView(withText(matcherText)).perform(clickOnClickableSpan(0));
        assertEquals(
                1,
                mUserActionTester.getActionCount(
                        "Settings.PrivacySandbox.AdTopics.PrivacyPolicyLinkClicked"));
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @DisableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_AD_TOPICS_CONTENT_PARITY)
    public void testRenderTopicsOffV2() throws IOException {
        setTopicsPrefEnabled(false);
        startTopicsSettings();
        mRenderTestRule.render(getTopicsRootView(), "topics_page_off_v2");
    }

    @Test
    @SmallTest
    public void testToggleUncheckedWhenTopicsOff() {
        setTopicsPrefEnabled(false);
        startTopicsSettings();
        onView(getTopicsToggleMatcher()).check(matches(not(isChecked())));
    }

    @Test
    @SmallTest
    public void testToggleCheckedWhenTopicsOn() {
        setTopicsPrefEnabled(true);
        startTopicsSettings();
        onView(getTopicsToggleMatcher()).check(matches(isChecked()));
    }

    @Test
    @SmallTest
    public void testTurnTopicsOnWhenTopicListEmpty() {
        setTopicsPrefEnabled(false);
        startTopicsSettings();
        onView(getTopicsToggleMatcher()).perform(click());

        assertTrue(mFakePrivacySandboxBridge.getLastTopicsToggleValue());
        assertTrue(isTopicsPrefEnabled());
        onViewWaiting(
                        withText(
                                R.string
                                        .settings_topics_page_current_topics_description_empty_text_v2))
                .check(matches(isDisplayed()));
        onView(withText(R.string.settings_topics_page_current_topics_description_disabled))
                .check(doesNotExist());

        assertThat(
                mUserActionTester.getActions(), hasItems("Settings.PrivacySandbox.Topics.Enabled"));
    }

    @Test
    @SmallTest
    public void testTurnTopicsOnWhenTopicsListPopulated() {
        setTopicsPrefEnabled(false);
        mFakePrivacySandboxBridge.setCurrentTopTopics(TOPIC_NAME_1, TOPIC_NAME_2);
        startTopicsSettings();

        // Check that the Topics list is not displayed when Topics are disabled.
        onView(withText(TOPIC_NAME_1)).check(doesNotExist());
        onView(withText(TOPIC_NAME_2)).check(doesNotExist());

        // Click on the toggle.
        onView(getTopicsToggleMatcher()).perform(click());
        assertTrue(mFakePrivacySandboxBridge.getLastTopicsToggleValue());

        // Check that the Topics list is displayed when Topics are enabled.
        onViewWaiting(withText(TOPIC_NAME_1)).check(matches(isDisplayed()));
        onView(withText(TOPIC_NAME_2)).check(matches(isDisplayed()));

        // Check that actions are reported
        assertThat(
                mUserActionTester.getActions(), hasItems("Settings.PrivacySandbox.Topics.Enabled"));
    }

    @Test
    @SmallTest
    public void testTurnTopicsOff() {
        setTopicsPrefEnabled(true);
        startTopicsSettings();
        onView(getTopicsToggleMatcher()).perform(click());
        assertFalse(mFakePrivacySandboxBridge.getLastTopicsToggleValue());

        assertFalse(isTopicsPrefEnabled());
        onView(withText(R.string.settings_topics_page_current_topics_description_empty_text_v2))
                .check(doesNotExist());

        assertThat(
                mUserActionTester.getActions(),
                hasItems("Settings.PrivacySandbox.Topics.Disabled"));
    }

    @Test
    @SmallTest
    public void testPopulateTopicsList() {
        setTopicsPrefEnabled(true);
        mFakePrivacySandboxBridge.setCurrentTopTopics(TOPIC_NAME_1, TOPIC_NAME_2);
        startTopicsSettings();

        onView(withText(TOPIC_NAME_1)).check(matches(isDisplayed()));
        onView(withText(TOPIC_NAME_2)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testBlockedTopicsAppearWhenTopicOn() {
        setTopicsPrefEnabled(true);
        mFakePrivacySandboxBridge.setBlockedTopics(TOPIC_NAME_1, TOPIC_NAME_2);
        startTopicsSettings();
        clickRecyclerViewItemWithText(mBlockedTopicsHeadingText);

        onViewWaiting(withText(mBlockedTopicsHeadingText));
        onView(withText(TOPIC_NAME_1)).check(matches(isDisplayed()));
        onView(withText(TOPIC_NAME_2)).check(matches(isDisplayed()));

        assertThat(
                mUserActionTester.getActions(),
                hasItems("Settings.PrivacySandbox.Topics.BlockedTopicsOpened"));
    }

    @Test
    @SmallTest
    public void testBlockTopics() {
        setTopicsPrefEnabled(true);
        mFakePrivacySandboxBridge.setCurrentTopTopics(TOPIC_NAME_1, TOPIC_NAME_2);
        startTopicsSettings();

        // Remove the first Topic from the list.
        clickImageButtonNextToText(TOPIC_NAME_1);
        onView(withText(TOPIC_NAME_1)).check(doesNotExist());
        onView(withText(R.string.settings_topics_page_block_topic_snackbar))
                .check(matches(isDisplayed()));

        // Remove the second Topic from the list.
        clickImageButtonNextToText(TOPIC_NAME_2);
        onView(withText(TOPIC_NAME_2)).check(doesNotExist());
        onView(withText(R.string.settings_topics_page_block_topic_snackbar))
                .check(matches(isDisplayed()));

        // Check that the empty state UI is displayed when the Topic list is empty.
        onView(withText(R.string.settings_topics_page_current_topics_description_empty_text_v2))
                .check(matches(isDisplayed()));

        // Open the blocked topics sub-page
        clickRecyclerViewItemWithText(mBlockedTopicsHeadingText);
        onViewWaiting(withText(mBlockedTopicsHeadingText));

        // Verify that the topics are blocked
        onView(withText(TOPIC_NAME_1)).check(matches(isDisplayed()));
        onView(withText(TOPIC_NAME_2)).check(matches(isDisplayed()));

        // Verify that actions are reported
        assertThat(
                mUserActionTester.getActions(),
                hasItems(
                        "Settings.PrivacySandbox.Topics.BlockedTopicsOpened",
                        "Settings.PrivacySandbox.Topics.TopicRemoved"));
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_AD_TOPICS_CONTENT_PARITY)
    public void testUnblockTopics() {
        setTopicsPrefEnabled(true);
        mFakePrivacySandboxBridge.setBlockedTopics(TOPIC_NAME_1, TOPIC_NAME_2);
        startTopicsSettings();

        // Open the blocked Topics sub-page
        clickRecyclerViewItemWithText(mBlockedTopicsHeadingText);
        onViewWaiting(withText(mBlockedTopicsHeadingText));

        // Unblock the first Topic
        clickImageButtonNextToText(TOPIC_NAME_1);
        onView(withText(TOPIC_NAME_1)).check(doesNotExist());

        // Unblock the second Topic
        clickImageButtonNextToText(TOPIC_NAME_2);
        onView(withText(TOPIC_NAME_2)).check(doesNotExist());

        // Check that the empty state UI is displayed when the Topic list is empty.
        onView(withText(R.string.settings_topics_page_blocked_topics_description_empty_text_v2))
                .check(matches(isDisplayed()));

        // Go back to the main Topics fragment
        pressBack();
        onViewWaiting(withText(R.string.settings_topics_page_toggle_sub_label_v2));

        // Verify that the Topics are unblocked
        onView(withText(TOPIC_NAME_1)).check(matches(isDisplayed()));
        onView(withText(TOPIC_NAME_2)).check(matches(isDisplayed()));

        // Verify that actions are sent
        assertThat(
                mUserActionTester.getActions(),
                hasItems(
                        "Settings.PrivacySandbox.Topics.BlockedTopicsOpened",
                        "Settings.PrivacySandbox.Topics.TopicAdded"));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_AD_TOPICS_CONTENT_PARITY)
    public void testUnblockTopicsAdTopicsContentParity() {
        setTopicsPrefEnabled(true);
        mFakePrivacySandboxBridge.setBlockedTopics(TOPIC_NAME_1, TOPIC_NAME_2);
        startTopicsSettings();

        // Open the blocked Topics sub-page
        clickRecyclerViewItemWithText(mBlockedTopicsHeadingText);
        onViewWaiting(withText(mBlockedTopicsHeadingText));

        // Unblock the first Topic
        clickImageButtonNextToText(TOPIC_NAME_1);
        onView(withText(TOPIC_NAME_1)).check(doesNotExist());

        // Unblock the second Topic
        clickImageButtonNextToText(TOPIC_NAME_2);
        onView(withText(TOPIC_NAME_2)).check(doesNotExist());

        // Check that the empty state UI is displayed when the Topic list is empty.
        onView(withText(R.string.settings_topics_page_blocked_topics_description_empty_text_v2))
                .check(matches(isDisplayed()));

        // Go back to the main Topics fragment
        pressBack();
        onViewWaiting(withText(R.string.settings_ad_topics_page_toggle_sub_label));

        // Verify that the Topics are unblocked
        onView(withText(TOPIC_NAME_1)).check(matches(isDisplayed()));
        onView(withText(TOPIC_NAME_2)).check(matches(isDisplayed()));

        // Verify that actions are sent
        assertThat(
                mUserActionTester.getActions(),
                hasItems(
                        "Settings.PrivacySandbox.Topics.BlockedTopicsOpened",
                        "Settings.PrivacySandbox.Topics.TopicAdded"));
    }

    @Test
    @SmallTest
    @Policies.Add({
        @Policies.Item(key = "PrivacySandboxAdTopicsEnabled", string = "false"),
        @Policies.Item(key = "PrivacySandboxPromptEnabled", string = "false")
    })
    public void testTopicsManaged() {
        startTopicsSettings();

        // Check default state and try to press the toggle.
        assertFalse(isTopicsPrefEnabled());
        onView(getTopicsToggleMatcher()).check(matches(not(isChecked())));
        onView(getTopicsToggleMatcher()).perform(click());
        assertFalse(mFakePrivacySandboxBridge.getLastTopicsToggleValue());

        // Check that the state of the pref and the toggle did not change.
        assertFalse(isTopicsPrefEnabled());
        onView(getTopicsToggleMatcher()).check(matches(not(isChecked())));
    }

    @Test
    @SmallTest
    public void displaysFledgeFooterLinkV2() throws IOException {
        setTopicsPrefEnabled(true);
        mFakePrivacySandboxBridge.setCurrentTopTopics(TOPIC_NAME_1, TOPIC_NAME_2);
        startTopicsSettings();
        TopicsFragment fragment = mSettingsActivityTestRule.getFragment();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    RecyclerView recyclerView = fragment.getView().findViewById(R.id.recycler_view);
                    recyclerView.scrollToPosition(recyclerView.getAdapter().getItemCount() - 1);
                });
        // Open a Fledge settings activity.
        onView(withText(containsString("suggested ads"))).perform(clickOnClickableSpan(0));
        onViewWaiting(withText(R.string.settings_site_suggested_ads_page_toggle_sub_label_v2))
                .check(matches(isDisplayed()));
        // Close the additional activity by navigating back.
        pressBack();
    }

    @Test
    @SmallTest
    public void testFooterCookieSettingsLink() throws IOException {
        setTopicsPrefEnabled(true);
        mFakePrivacySandboxBridge.setCurrentTopTopics(TOPIC_NAME_1, TOPIC_NAME_2);
        startTopicsSettings();
        TopicsFragment fragment = mSettingsActivityTestRule.getFragment();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    RecyclerView recyclerView = fragment.getView().findViewById(R.id.recycler_view);
                    recyclerView.scrollToPosition(recyclerView.getAdapter().getItemCount() - 1);
                });
        // Open a CookieSettings activity.
        onView(withText(containsString("cookie settings"))).perform(clickOnClickableSpan(1));
        onViewWaiting(withText(R.string.third_party_cookies_page_title))
                .check(matches(isDisplayed()));
        // Close the additional activity by navigating back.
        pressBack();
    }

    @Test
    @SmallTest
    public void testTopicsIconsExist() {
        var activity = mSettingsActivityTestRule.startSettingsActivity();
        PrivacySandboxBridgeJni.setInstanceForTesting(new PrivacySandboxBridgeJni());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PrivacySandboxBridge privacySandboxBridge =
                            new PrivacySandboxBridge(ProfileManager.getLastUsedRegularProfile());
                    for (var topic : privacySandboxBridge.getFirstLevelTopics()) {
                        int iconId = TopicsUtils.getIconResourceIdForTopic(activity, topic);
                        assertTrue(
                                "Topic drawable icon not found for: " + topic.getName(),
                                iconId != 0);
                    }
                });
    }

    /* Ad Topics Content Parity Tests */
    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_AD_TOPICS_CONTENT_PARITY)
    public void testRenderTopicsOffV2AdTopicsContentParity() throws IOException {
        setTopicsPrefEnabled(false);
        startTopicsSettings();
        mRenderTestRule.render(
                getTopicsRootViewAdTopicsContentParity(),
                "topics_page_off_v2_ad_topics_content_parity");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_AD_TOPICS_CONTENT_PARITY)
    public void testRenderTopicsEmptyAdTopicsContentParity() throws IOException {
        setTopicsPrefEnabled(true);
        startTopicsSettings();
        mRenderTestRule.render(
                getTopicsRootViewAdTopicsContentParity(),
                "topics_page_empty_ad_topics_content_parity");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_AD_TOPICS_CONTENT_PARITY)
    public void testRenderTopicsPopulatedAdTopicsContentParity() throws IOException {
        setTopicsPrefEnabled(true);
        mFakePrivacySandboxBridge.setCurrentTopTopics(TOPIC_NAME_1, TOPIC_NAME_2);
        startTopicsSettings();
        mRenderTestRule.render(
                getTopicsRootViewAdTopicsContentParity(),
                "topic_page_populated_ad_topics_content_parity");
    }
}
