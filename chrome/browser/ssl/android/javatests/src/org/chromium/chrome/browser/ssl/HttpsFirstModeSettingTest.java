// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ssl;

import static org.chromium.base.test.util.Batch.PER_CLASS;

import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.LargeTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.privacy.settings.PrivacySettings;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.concurrent.ExecutionException;

/**
 * Tests for the HTTPS-First Mode setting in Privacy and security. Enables the
 * HTTPS_FIRST_BALANCED_MODE feature flag to test the new settings UI.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(ChromeFeatureList.HTTPS_FIRST_BALANCED_MODE)
@Batch(PER_CLASS)
public class HttpsFirstModeSettingTest {
    private final SettingsActivityTestRule<PrivacySettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(PrivacySettings.class);

    private static final String PREF_HTTPS_FIRST_MODE = "https_first_mode";
    private static final String PREF_HTTPS_FIRST_MODE_SWITCH = "https_first_mode_switch";
    private static final String PREF_HTTPS_FIRST_MODE_VARIANT = "https_first_mode_variant";

    private static Preference waitForPreference(
            final PreferenceFragmentCompat prefFragment, final String preferenceKey)
            throws ExecutionException {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Expected valid preference for: " + preferenceKey,
                            prefFragment.findPreference(preferenceKey),
                            Matchers.notNullValue());
                });

        return ThreadUtils.runOnUiThreadBlocking(() -> prefFragment.findPreference(preferenceKey));
    }

    @Test
    @LargeTest
    @EnableFeatures("HttpsFirstBalancedModeAutoEnable")
    public void testSetting_AdvancedProtectionDisabled() throws Exception {
        mSettingsActivityTestRule.startSettingsActivity();
        final PrivacySettings privacySettings = mSettingsActivityTestRule.getFragment();

        Preference pref = waitForPreference(privacySettings, PREF_HTTPS_FIRST_MODE);
        Assert.assertNotNull(pref);

        // Check that the expected title is shown and the setting defaults to balanced.
        final String prefTitle =
                ApplicationProvider.getApplicationContext()
                        .getString(R.string.settings_https_first_mode_title);
        final String prefSummary =
                ApplicationProvider.getApplicationContext()
                        .getString(R.string.settings_https_first_mode_enabled_balanced_label);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(pref.getTitle().equals(prefTitle));
                    Assert.assertTrue(pref.getSummary().equals(prefSummary));
                });
    }

    @Test
    @LargeTest
    @CommandLineFlags.Add({"safe-browsing-treat-user-as-advanced-protection"})
    public void testSetting_AdvancedProtectionEnabled() throws Exception {
        mSettingsActivityTestRule.startSettingsActivity();

        // Checks that when the user is enrolled in APP, the summary shows that
        // the user is in strict mode.
        final PrivacySettings privacySettings = mSettingsActivityTestRule.getFragment();
        final String lockedSummaryText =
                ApplicationProvider.getApplicationContext()
                        .getString(R.string.settings_https_first_mode_enabled_strict_label);

        Preference pref = waitForPreference(privacySettings, PREF_HTTPS_FIRST_MODE);
        Assert.assertNotNull(pref);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(pref.getSummary().equals(lockedSummaryText));
                });
    }

    @Test
    @LargeTest
    @DisableFeatures(ChromeFeatureList.HTTPS_FIRST_BALANCED_MODE)
    public void testSetting_NewSettingNotShownWhenFeatureDisabled() throws Exception {
        // Check that the new setting preference isn't visible.
        mSettingsActivityTestRule.startSettingsActivity();

        final PrivacySettings privacySettings = mSettingsActivityTestRule.getFragment();

        Preference pref = waitForPreference(privacySettings, PREF_HTTPS_FIRST_MODE);
        Assert.assertNotNull(pref);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertFalse(pref.isVisible());
                });
    }

    @Test
    @LargeTest
    @EnableFeatures(ChromeFeatureList.HTTPS_FIRST_BALANCED_MODE)
    public void testSetting_OldSettingNotShownWhenFeatureEnabled() throws Exception {
        // Check that the old setting preference isn't visible.
        mSettingsActivityTestRule.startSettingsActivity();

        final PrivacySettings privacySettings = mSettingsActivityTestRule.getFragment();

        Preference pref = waitForPreference(privacySettings, "https_first_mode_legacy");
        Assert.assertNotNull(pref);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertFalse(pref.isVisible());
                });
    }
}
