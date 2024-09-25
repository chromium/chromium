// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.privacy_sandbox;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.assertThat;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.hasItems;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.chrome.browser.privacy_sandbox.AdMeasurementFragment.setAdMeasurementPrefEnabled;
import static org.chromium.chrome.browser.privacy_sandbox.FledgeFragment.setFledgePrefEnabled;
import static org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxTestUtils.getRootViewSanitized;
import static org.chromium.chrome.browser.privacy_sandbox.TopicsFragment.setTopicsPrefEnabled;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.os.Bundle;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;

/** Tests {@link PrivacySandboxSettingsFragment} */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public final class PrivacySandboxSettingsFragmentTest {
    @Rule public ChromeBrowserTestRule mChromeBrowserTestRule = new ChromeBrowserTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(RenderTestRule.Component.UI_SETTINGS_PRIVACY)
                    .build();

    @Rule
    public SettingsActivityTestRule<PrivacySandboxSettingsFragment> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(PrivacySandboxSettingsFragment.class);

    public UserActionTester mUserActionTester;

    @Before
    public void setUp() {
        mUserActionTester = new UserActionTester();
    }

    @After
    public void tearDown() {
        runOnUiThreadBlocking(
                () -> {
                    PrefService prefService =
                            UserPrefs.get(ProfileManager.getLastUsedRegularProfile());
                    prefService.clearPref(Pref.PRIVACY_SANDBOX_M1_TOPICS_ENABLED);
                    prefService.clearPref(Pref.PRIVACY_SANDBOX_M1_FLEDGE_ENABLED);
                    prefService.clearPref(Pref.PRIVACY_SANDBOX_M1_AD_MEASUREMENT_ENABLED);
                });

        mUserActionTester.tearDown();
    }

    private void startPrivacySandboxSettingsV4() {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putInt(
                PrivacySandboxSettingsFragment.PRIVACY_SANDBOX_REFERRER,
                PrivacySandboxReferrer.PRIVACY_SETTINGS);
        mSettingsActivityTestRule.startSettingsActivity(fragmentArgs);
        onViewWaiting(withText(R.string.ad_privacy_page_title));
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderPrivacySandboxSettingsV4() throws IOException {
        startPrivacySandboxSettingsV4();
        mRenderTestRule.render(
                getRootViewSanitized(R.string.ad_privacy_page_title),
                "privacy_sandbox_settings_v4");
    }

    @Test
    @SmallTest
    public void testTopicsPrefDisabledDescription() {
        runOnUiThreadBlocking(
                () -> setTopicsPrefEnabled(ProfileManager.getLastUsedRegularProfile(), false));
        startPrivacySandboxSettingsV4();

        onView(withText(R.string.ad_privacy_page_topics_link_row_sub_label_disabled))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testTopicsPrefEnabledDescription() {
        runOnUiThreadBlocking(
                () -> setTopicsPrefEnabled(ProfileManager.getLastUsedRegularProfile(), true));
        startPrivacySandboxSettingsV4();

        onView(withText(R.string.ad_privacy_page_topics_link_row_sub_label_enabled))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testFledgePrefDisabledDescription() {
        runOnUiThreadBlocking(
                () -> setFledgePrefEnabled(ProfileManager.getLastUsedRegularProfile(), false));
        startPrivacySandboxSettingsV4();

        onView(withText(R.string.ad_privacy_page_fledge_link_row_sub_label_disabled))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testFledgePrefEnabledDescription() {
        runOnUiThreadBlocking(
                () -> setFledgePrefEnabled(ProfileManager.getLastUsedRegularProfile(), true));
        startPrivacySandboxSettingsV4();

        onView(withText(R.string.ad_privacy_page_fledge_link_row_sub_label_enabled))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testAdMeasurementPrefDisabledDescription() {
        runOnUiThreadBlocking(
                () ->
                        setAdMeasurementPrefEnabled(
                                ProfileManager.getLastUsedRegularProfile(), false));
        startPrivacySandboxSettingsV4();

        onView(withText(R.string.ad_privacy_page_ad_measurement_link_row_sub_label_disabled))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testAdMeasurementPrefEnabledDescription() {
        runOnUiThreadBlocking(
                () ->
                        setAdMeasurementPrefEnabled(
                                ProfileManager.getLastUsedRegularProfile(), true));
        startPrivacySandboxSettingsV4();

        onView(withText(R.string.ad_privacy_page_ad_measurement_link_row_sub_label_enabled))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testNavigateToTopicsPage() {
        startPrivacySandboxSettingsV4();
        onView(withText(R.string.ad_privacy_page_topics_link_row_label)).perform(click());

        onViewWaiting(withText(R.string.settings_topics_page_toggle_sub_label_v2));
        assertThat(
                mUserActionTester.getActions(), hasItems("Settings.PrivacySandbox.Topics.Opened"));
    }

    @Test
    @SmallTest
    public void testNavigateToFledgePage() {
        startPrivacySandboxSettingsV4();
        onView(withText(R.string.ad_privacy_page_fledge_link_row_label)).perform(click());

        onViewWaiting(withText(R.string.settings_fledge_page_toggle_sub_label));
        assertThat(
                mUserActionTester.getActions(), hasItems("Settings.PrivacySandbox.Fledge.Opened"));
    }

    @Test
    @SmallTest
    public void testNavigateToAdMeasurementPage() {
        startPrivacySandboxSettingsV4();
        onView(withText(R.string.ad_privacy_page_ad_measurement_link_row_label)).perform(click());

        onViewWaiting(withText(R.string.settings_ad_measurement_page_toggle_sub_label));
        assertThat(
                mUserActionTester.getActions(),
                hasItems("Settings.PrivacySandbox.AdMeasurement.Opened"));
    }
}
