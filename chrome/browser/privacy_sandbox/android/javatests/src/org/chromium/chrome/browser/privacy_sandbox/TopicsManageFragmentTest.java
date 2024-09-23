// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.assertThat;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.containsString;
import static org.hamcrest.Matchers.hasItems;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/** Tests {@link TopicsManageFragment} */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public final class TopicsManageFragmentTest {
    private static final String TOPIC_NAME_1 = "Topic 1";

    @Rule public ChromeBrowserTestRule mChromeBrowserTestRule = new ChromeBrowserTestRule();

    @Rule
    public SettingsActivityTestRule<TopicsManageFragment> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(TopicsManageFragment.class);

    @Rule public JniMocker mocker = new JniMocker();

    private FakePrivacySandboxBridge mFakePrivacySandboxBridge;
    private UserActionTester mUserActionTester;

    @Before
    public void setUp() {
        mFakePrivacySandboxBridge = new FakePrivacySandboxBridge();
        mocker.mock(PrivacySandboxBridgeJni.TEST_HOOKS, mFakePrivacySandboxBridge);

        mUserActionTester = new UserActionTester();
    }

    @After
    public void tearDown() {
        mUserActionTester.tearDown();
    }

    private void startTopicsManageSettings() {
        mSettingsActivityTestRule.startSettingsActivity();
        onViewWaiting(withText(containsString("Choose which broad categories")));
    }

    @Test
    @SmallTest
    public void testOpenManagePage() {
        startTopicsManageSettings();
        assertThat(
                mUserActionTester.getActions(),
                hasItems("Settings.PrivacySandbox.Topics.Manage.PageOpened"));
    }

    @Test
    @SmallTest
    public void testToggleTopic() {
        mFakePrivacySandboxBridge.setFirstLevelTopics(TOPIC_NAME_1);
        startTopicsManageSettings();
        onView(withText(TOPIC_NAME_1)).perform(click());
        assertThat(
                mUserActionTester.getActions(),
                hasItems("Settings.PrivacySandbox.Topics.Manage.TopicBlocked"));
        onView(withText(TOPIC_NAME_1)).perform(click());
        assertThat(
                mUserActionTester.getActions(),
                hasItems("Settings.PrivacySandbox.Topics.Manage.TopicEnabled"));
    }

    @Test
    @SmallTest
    public void testToggleTopicDialog() {
        mFakePrivacySandboxBridge.setFirstLevelTopics(TOPIC_NAME_1);
        mFakePrivacySandboxBridge.setChildTopics(TOPIC_NAME_1);
        startTopicsManageSettings();
        onView(withText(TOPIC_NAME_1)).perform(click());
        onViewWaiting(withText("Cancel")).check(matches(isDisplayed()));
        onView(withText("Block")).perform(click());
        assertThat(
                mUserActionTester.getActions(),
                hasItems("Settings.PrivacySandbox.Topics.Manage.TopicBlockingConfirmed"));
        onView(withText(TOPIC_NAME_1)).perform(click());
        assertThat(
                mUserActionTester.getActions(),
                hasItems("Settings.PrivacySandbox.Topics.Manage.TopicEnabled"));
    }
}
