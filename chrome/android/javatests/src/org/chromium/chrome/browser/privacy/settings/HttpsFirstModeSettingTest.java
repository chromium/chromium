// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy.settings;

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
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;

import java.util.concurrent.ExecutionException;

/** Tests for the HTTPS-First Mode setting in Privacy and security. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(PER_CLASS)
public class HttpsFirstModeSettingTest {
    private final SettingsActivityTestRule<PrivacySettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(PrivacySettings.class);

    private static final String PREF_HTTPS_FIRST_MODE = "https_first_mode";

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
    public void testSetting_AdvancedProtectionDisabled() throws Exception {
        mSettingsActivityTestRule.startSettingsActivity();
        final PrivacySettings privacySettings = mSettingsActivityTestRule.getFragment();
        final String unlockedSummaryText =
                ApplicationProvider.getApplicationContext()
                        .getResources()
                        .getString(R.string.settings_https_first_mode_summary);

        Preference pref = waitForPreference(privacySettings, PREF_HTTPS_FIRST_MODE);
        Assert.assertNotNull(pref);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(pref.getSummary().equals(unlockedSummaryText));
                });
    }

    @Test
    @LargeTest
    @CommandLineFlags.Add({"safe-browsing-treat-user-as-advanced-protection"})
    public void testSetting_AdvancedProtectionEnabled() throws Exception {
        mSettingsActivityTestRule.startSettingsActivity();

        final PrivacySettings privacySettings = mSettingsActivityTestRule.getFragment();
        final String lockedSummaryText =
                ApplicationProvider.getApplicationContext()
                        .getResources()
                        .getString(
                                R.string
                                        .settings_https_first_mode_with_advanced_protection_summary);

        Preference pref = waitForPreference(privacySettings, PREF_HTTPS_FIRST_MODE);
        Assert.assertNotNull(pref);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(pref.getSummary().equals(lockedSummaryText));
                });
    }
}
