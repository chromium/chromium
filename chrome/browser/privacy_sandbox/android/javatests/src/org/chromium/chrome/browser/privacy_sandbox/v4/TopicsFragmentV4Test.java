// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox.v4;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.isChecked;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.view.View;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matcher;
import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.privacy_sandbox.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.ViewUtils;

/**
 * Tests {@link TopicsFragmentV4}
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4)
public final class TopicsFragmentV4Test {
    @Rule
    public ChromeBrowserTestRule mChromeBrowserTestRule = new ChromeBrowserTestRule();

    @Rule
    public SettingsActivityTestRule<TopicsFragmentV4> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(TopicsFragmentV4.class);

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PrefService prefService = UserPrefs.get(Profile.getLastUsedRegularProfile());
            prefService.clearPref(Pref.PRIVACY_SANDBOX_M1_TOPICS_ENABLED);
        });
    }

    private void startTopicsSettings() {
        mSettingsActivityTestRule.startSettingsActivity();
        ViewUtils.onViewWaiting(withText(R.string.settings_topics_page_title));
    }

    private Matcher<View> getTopicsToggleMatcher() {
        return allOf(withId(R.id.switchWidget),
                withParent(withParent(
                        hasDescendant(withText(R.string.settings_topics_page_toggle_label)))));
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
    public void testTurnTopicsOn() {
        setTopicsPrefEnabled(false);
        startTopicsSettings();
        onView(getTopicsToggleMatcher()).perform(click());
        assertTrue(isTopicsPrefEnabled());
    }

    @Test
    @SmallTest
    public void testTurnTopicsOff() {
        setTopicsPrefEnabled(true);
        startTopicsSettings();
        onView(getTopicsToggleMatcher()).perform(click());
        assertFalse(isTopicsPrefEnabled());
    }
}
