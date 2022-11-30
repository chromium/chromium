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

import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.test.util.ViewUtils;

/**
 * Tests {@link PrivacyGuideFragment}
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PrivacyGuideFragmentTest {
    @Rule
    public SettingsActivityTestRule<PrivacyGuideFragment> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(PrivacyGuideFragment.class);

    private void launchPrivacyGuide() {
        mSettingsActivityTestRule.startSettingsActivity();
        ViewUtils.onViewWaiting(withText(R.string.prefs_privacy_guide_title));
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
}