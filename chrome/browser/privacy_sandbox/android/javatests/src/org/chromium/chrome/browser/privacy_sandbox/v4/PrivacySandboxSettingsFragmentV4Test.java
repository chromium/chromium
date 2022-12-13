// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.privacy_sandbox.v4;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxTestUtils.getRootViewSanitized;
import static org.chromium.chrome.browser.privacy_sandbox.v4.AdMeasurementFragmentV4.setAdMeasurementPrefEnabled;
import static org.chromium.chrome.browser.privacy_sandbox.v4.FledgeFragmentV4.setFledgePrefEnabled;
import static org.chromium.chrome.browser.privacy_sandbox.v4.TopicsFragmentV4.setTopicsPrefEnabled;
import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.os.Bundle;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxReferrer;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxSettingsFragment;
import org.chromium.chrome.browser.privacy_sandbox.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;

/**
 * Tests {@link PrivacySandboxSettingsFragmentV4}
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4)
public final class PrivacySandboxSettingsFragmentV4Test {
    @Rule
    public ChromeBrowserTestRule mChromeBrowserTestRule = new ChromeBrowserTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(RenderTestRule.Component.UI_SETTINGS_PRIVACY)
                    .build();

    @Rule
    public SettingsActivityTestRule<PrivacySandboxSettingsFragmentV4> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(PrivacySandboxSettingsFragmentV4.class);

    @After
    public void tearDown() {
        runOnUiThreadBlocking(() -> {
            PrefService prefService = UserPrefs.get(Profile.getLastUsedRegularProfile());
            prefService.clearPref(Pref.PRIVACY_SANDBOX_M1_TOPICS_ENABLED);
            prefService.clearPref(Pref.PRIVACY_SANDBOX_M1_FLEDGE_ENABLED);
            prefService.clearPref(Pref.PRIVACY_SANDBOX_M1_AD_MEASUREMENT_ENABLED);
        });
    }

    private void startPrivacySandboxSettingsV4() {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putInt(PrivacySandboxSettingsFragment.PRIVACY_SANDBOX_REFERRER,
                PrivacySandboxReferrer.PRIVACY_SETTINGS);
        mSettingsActivityTestRule.startSettingsActivity(fragmentArgs);
        onViewWaiting(withText(R.string.ad_privacy_page_title));
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderPrivacySandboxSettingsV4() throws IOException {
        startPrivacySandboxSettingsV4();
        mRenderTestRule.render(getRootViewSanitized(R.string.ad_privacy_page_title),
                "privacy_sandbox_settings_v4");
    }

    @Test
    @SmallTest
    public void testTopicsPrefDisabledDescription() {
        runOnUiThreadBlocking(() -> setTopicsPrefEnabled(false));
        startPrivacySandboxSettingsV4();

        onView(withText(R.string.ad_privacy_page_topics_link_row_sub_label_disabled))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testTopicsPrefEnabledDescription() {
        runOnUiThreadBlocking(() -> setTopicsPrefEnabled(true));
        startPrivacySandboxSettingsV4();

        onView(withText(R.string.ad_privacy_page_topics_link_row_sub_label_enabled))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testFledgePrefDisabledDescription() {
        runOnUiThreadBlocking(() -> setFledgePrefEnabled(false));
        startPrivacySandboxSettingsV4();

        onView(withText(R.string.ad_privacy_page_fledge_link_row_sub_label_disabled))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testFledgePrefEnabledDescription() {
        runOnUiThreadBlocking(() -> setFledgePrefEnabled(true));
        startPrivacySandboxSettingsV4();

        onView(withText(R.string.ad_privacy_page_fledge_link_row_sub_label_enabled))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testAdMeasurementPrefDisabledDescription() {
        runOnUiThreadBlocking(() -> setAdMeasurementPrefEnabled(false));
        startPrivacySandboxSettingsV4();

        onView(withText(R.string.ad_privacy_page_ad_measurement_link_row_sub_label_disabled))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testAdMeasurementPrefEnabledDescription() {
        runOnUiThreadBlocking(() -> setAdMeasurementPrefEnabled(true));
        startPrivacySandboxSettingsV4();

        onView(withText(R.string.ad_privacy_page_ad_measurement_link_row_sub_label_enabled))
                .check(matches(isDisplayed()));
    }
}
