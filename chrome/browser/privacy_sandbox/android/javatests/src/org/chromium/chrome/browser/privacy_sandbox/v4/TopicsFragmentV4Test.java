// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox.v4;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.Espresso.pressBack;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.isChecked;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxTestUtils.clickImageButtonNextToText;
import static org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxTestUtils.getRootViewSanitized;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.view.View;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matcher;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.privacy_sandbox.FakePrivacySandboxBridge;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxBridgeJni;
import org.chromium.chrome.browser.privacy_sandbox.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;

/**
 * Tests {@link TopicsFragmentV4}
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4)
public final class TopicsFragmentV4Test {
    private static final String TOPIC_NAME_1 = "Topic 1";
    private static final String TOPIC_NAME_2 = "Topic 2";

    @Rule
    public ChromeBrowserTestRule mChromeBrowserTestRule = new ChromeBrowserTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(RenderTestRule.Component.UI_SETTINGS_PRIVACY)
                    .build();

    @Rule
    public SettingsActivityTestRule<TopicsFragmentV4> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(TopicsFragmentV4.class);

    @Rule
    public JniMocker mocker = new JniMocker();

    private FakePrivacySandboxBridge mFakePrivacySandboxBridge;

    @Before
    public void setUp() {
        mFakePrivacySandboxBridge = new FakePrivacySandboxBridge();
        mocker.mock(PrivacySandboxBridgeJni.TEST_HOOKS, mFakePrivacySandboxBridge);
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PrefService prefService = UserPrefs.get(Profile.getLastUsedRegularProfile());
            prefService.clearPref(Pref.PRIVACY_SANDBOX_M1_TOPICS_ENABLED);
        });
    }

    private void startTopicsSettings() {
        mSettingsActivityTestRule.startSettingsActivity();
        onViewWaiting(withText(R.string.settings_topics_page_title));
    }

    private Matcher<View> getTopicsToggleMatcher() {
        return allOf(withId(R.id.switchWidget),
                withParent(withParent(
                        hasDescendant(withText(R.string.settings_topics_page_toggle_label)))));
    }

    private View getTopicsRootView() {
        return getRootViewSanitized(R.string.settings_topics_page_title);
    }

    private View getBlockedTopicsRootView() {
        return getRootViewSanitized(R.string.settings_topics_page_blocked_topics_sub_page_title);
    }

    private void setTopicsPrefEnabled(boolean isEnabled) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> TopicsFragmentV4.setTopicsPrefEnabled(isEnabled));
    }

    private boolean isTopicsPrefEnabled() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> TopicsFragmentV4.isTopicsPrefEnabled());
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderTopicsOff() throws IOException {
        setTopicsPrefEnabled(false);
        startTopicsSettings();
        mRenderTestRule.render(getTopicsRootView(), "topics_page_off");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderTopicsEmpty() throws IOException {
        setTopicsPrefEnabled(true);
        startTopicsSettings();
        mRenderTestRule.render(getTopicsRootView(), "topics_page_empty");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
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
        setTopicsPrefEnabled(false);
        startTopicsSettings();
        onView(withText(R.string.settings_topics_page_blocked_topics_heading)).perform(click());
        mRenderTestRule.render(getBlockedTopicsRootView(), "blocked_topics_page_empty");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderBlockedTopicsOffPopulated() throws IOException {
        setTopicsPrefEnabled(false);
        mFakePrivacySandboxBridge.setBlockedTopics(TOPIC_NAME_1, TOPIC_NAME_2);
        startTopicsSettings();
        onView(withText(R.string.settings_topics_page_blocked_topics_heading)).perform(click());
        mRenderTestRule.render(getBlockedTopicsRootView(), "blocked_topics_page_off_populated");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderBlockedTopicsOnPopulated() throws IOException {
        setTopicsPrefEnabled(true);
        mFakePrivacySandboxBridge.setBlockedTopics(TOPIC_NAME_1, TOPIC_NAME_2);
        startTopicsSettings();
        onView(withText(R.string.settings_topics_page_blocked_topics_heading)).perform(click());
        mRenderTestRule.render(getBlockedTopicsRootView(), "blocked_topics_page_on_populated");
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

        assertTrue(isTopicsPrefEnabled());
        onViewWaiting(withText(R.string.settings_topics_page_current_topics_description_empty))
                .check(matches(isDisplayed()));
        onView(withText(R.string.settings_topics_page_current_topics_description_disabled))
                .check(doesNotExist());
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

        // Check that the Topics list is displayed when Topics are enabled.
        onViewWaiting(withText(TOPIC_NAME_1)).check(matches(isDisplayed()));
        onView(withText(TOPIC_NAME_2)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testTurnTopicsOff() {
        setTopicsPrefEnabled(true);
        startTopicsSettings();
        onView(getTopicsToggleMatcher()).perform(click());

        assertFalse(isTopicsPrefEnabled());
        onViewWaiting(withText(R.string.settings_topics_page_current_topics_description_disabled))
                .check(matches(isDisplayed()));
        onView(withText(R.string.settings_topics_page_current_topics_description_empty))
                .check(doesNotExist());
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
    public void testBlockedTopicsAppearWhenTopicOff() {
        setTopicsPrefEnabled(false);
        mFakePrivacySandboxBridge.setBlockedTopics(TOPIC_NAME_1, TOPIC_NAME_2);
        startTopicsSettings();
        onView(withText(R.string.settings_topics_page_blocked_topics_heading)).perform(click());

        onViewWaiting(withText(R.string.settings_topics_page_blocked_topics_description_disabled));
        onView(withText(TOPIC_NAME_1)).check(matches(isDisplayed()));
        onView(withText(TOPIC_NAME_2)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testBlockedTopicsAppearWhenTopicOn() {
        setTopicsPrefEnabled(true);
        mFakePrivacySandboxBridge.setBlockedTopics(TOPIC_NAME_1, TOPIC_NAME_2);
        startTopicsSettings();
        onView(withText(R.string.settings_topics_page_blocked_topics_heading)).perform(click());

        onViewWaiting(withText(R.string.settings_topics_page_blocked_topics_description));
        onView(withText(TOPIC_NAME_1)).check(matches(isDisplayed()));
        onView(withText(TOPIC_NAME_2)).check(matches(isDisplayed()));
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

        // Remove the second Topic from the list.
        clickImageButtonNextToText(TOPIC_NAME_2);
        onView(withText(TOPIC_NAME_2)).check(doesNotExist());

        // Check that the empty state UI is displayed when the Topic list is empty.
        onView(withText(R.string.settings_topics_page_current_topics_description_empty))
                .check(matches(isDisplayed()));

        // Open the blocked topics sub-page
        onView(withText(R.string.settings_topics_page_blocked_topics_heading)).perform(click());
        onViewWaiting(withText(R.string.settings_topics_page_blocked_topics_sub_page_title));

        // Verify that the topics are blocked
        onView(withText(TOPIC_NAME_1)).check(matches(isDisplayed()));
        onView(withText(TOPIC_NAME_2)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testUnblockTopics() {
        setTopicsPrefEnabled(true);
        mFakePrivacySandboxBridge.setBlockedTopics(TOPIC_NAME_1, TOPIC_NAME_2);
        startTopicsSettings();

        // Open the blocked Topics sub-page
        onView(withText(R.string.settings_topics_page_blocked_topics_heading)).perform(click());
        onViewWaiting(withText(R.string.settings_topics_page_blocked_topics_sub_page_title));

        // Unblock the first Topic
        clickImageButtonNextToText(TOPIC_NAME_1);
        onView(withText(TOPIC_NAME_1)).check(doesNotExist());

        // Unblock the second Topic
        clickImageButtonNextToText(TOPIC_NAME_2);
        onView(withText(TOPIC_NAME_2)).check(doesNotExist());

        // Check that the empty state UI is displayed when the Topic list is empty.
        onView(withText(R.string.settings_topics_page_blocked_topics_description_empty))
                .check(matches(isDisplayed()));

        // Go back to the main Topics fragment
        pressBack();
        onViewWaiting(withText(R.string.settings_topics_page_title));

        // Verify that the Topics are unblocked
        onView(withText(TOPIC_NAME_1)).check(matches(isDisplayed()));
        onView(withText(TOPIC_NAME_2)).check(matches(isDisplayed()));
    }
    // TODO(http://b/261822498): Add Managed state tests when the Privacy Sandbox policy is
    // implemented.
}
