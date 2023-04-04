// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import android.os.Bundle;
import android.view.View;

import androidx.test.espresso.ViewInteraction;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.feed.test.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 *  Unit tests for {@link FeedAutoplaySettingsFragment}
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class FeedAutoplaySettingsFragmentTest {
    private static final String VIDEO_PREVIEWS_PREF_KEY = "ntp_snippets.video_previews_type";

    @Rule
    public SettingsActivityTestRule<FeedAutoplaySettingsFragment> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(FeedAutoplaySettingsFragment.class);

    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    private Profile mProfile;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    private void launchSettings() {
        mSettingsActivityTestRule.startSettingsActivity(new Bundle());
        FeedAutoplaySettingsFragment mFeedAutoplaySettingsFragment =
                mSettingsActivityTestRule.getFragment();
    }

    private PrefService getPrefService() {
        mProfile = Profile.getLastUsedRegularProfile();
        return UserPrefs.get(mProfile);
    }

    private void assertChecked(View view) {
        Assert.assertTrue(((RadioButtonWithDescription) view).isChecked());
    }

    private void assertUnchecked(View view) {
        Assert.assertFalse(((RadioButtonWithDescription) view).isChecked());
    }

    private ViewInteraction neverRadioButton() {
        return onView(withId(R.id.video_previews_option_never_radio_button));
    }

    private ViewInteraction wifiRadioButton() {
        return onView(withId(R.id.video_previews_option_wifi_radio_button));
    }

    private ViewInteraction wifiAndMobileDataRadioButton() {
        return onView(withId(R.id.video_previews_option_wifi_and_mobile_data_radio_button));
    }

    @Test
    @SmallTest
    public void testUserSelectsNeverRadioButton() {
        launchSettings();

        // "Wi-Fi" radio button is checked by default.
        neverRadioButton().check((view, e) -> assertUnchecked(view));
        wifiRadioButton().check((view, e) -> assertChecked(view));
        wifiAndMobileDataRadioButton().check((view, e) -> assertUnchecked(view));

        // User selects "Never" radio button
        neverRadioButton().perform(click());

        // Now "Never" radio button is checked.
        neverRadioButton().check((view, e) -> assertChecked(view));
        wifiRadioButton().check((view, e) -> assertUnchecked(view));
        wifiAndMobileDataRadioButton().check((view, e) -> assertUnchecked(view));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(0, getPrefService().getInteger(VIDEO_PREVIEWS_PREF_KEY));
        });
    }

    @Test
    @SmallTest
    public void testUserSelectsWifiAndMobileDataRadioButton() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { getPrefService().setInteger(VIDEO_PREVIEWS_PREF_KEY, 0); });

        launchSettings();

        // "Never" radio button is checked per prefs.
        neverRadioButton().check((view, e) -> assertChecked(view));
        wifiRadioButton().check((view, e) -> assertUnchecked(view));
        wifiAndMobileDataRadioButton().check((view, e) -> assertUnchecked(view));

        // User selects "Wi-Fi and mobile data" radio button
        wifiAndMobileDataRadioButton().perform(click());

        // Now "Wi-Fi and mobile data" radio button is checked.
        neverRadioButton().check((view, e) -> assertUnchecked(view));
        wifiRadioButton().check((view, e) -> assertUnchecked(view));
        wifiAndMobileDataRadioButton().check((view, e) -> assertChecked(view));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(2, getPrefService().getInteger(VIDEO_PREVIEWS_PREF_KEY));
        });
    }
}
