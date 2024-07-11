// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import androidx.preference.Preference;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matcher;
import org.junit.Assert;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Matchers;
import org.chromium.chrome.browser.download.settings.DownloadSettings;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

/** Test for download settings. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class DownloadSettingsTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    @Rule
    public final SettingsActivityTestRule<DownloadSettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(DownloadSettings.class);

    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(sActivityTestRule).around(mSettingsActivityTestRule);

    private Preference assertPreference(final String preferenceKey) throws Exception {
        return assertPreference(preferenceKey, Matchers.notNullValue());
    }

    private Preference assertPreference(final String preferenceKey, Matcher<Object> matcher)
            throws Exception {
        DownloadSettings downloadSettings = mSettingsActivityTestRule.getFragment();
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Expected valid preference for: " + preferenceKey,
                            downloadSettings.findPreference(preferenceKey),
                            matcher);
                });

        return ThreadUtils.runOnUiThreadBlocking(
                () -> downloadSettings.findPreference(preferenceKey));
    }

    private void waitForPolicyReady() {
        // Policy data from the annotation needs to be populated before the setting UI is opened.
        CriteriaHelper.pollUiThread(
                () ->
                        DownloadDialogBridge.isLocationDialogManaged(
                                ProfileManager.getLastUsedRegularProfile()));
    }

    private void verifyLocationPromptPolicy(boolean promptForDownload) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(
                            DownloadDialogBridge.isLocationDialogManaged(
                                    ProfileManager.getLastUsedRegularProfile()));
                    Assert.assertTrue(
                            getPrefService().isManagedPreference(Pref.PROMPT_FOR_DOWNLOAD));
                    DownloadSettings downloadSettings = mSettingsActivityTestRule.getFragment();
                    ChromeSwitchPreference locationPromptPreference =
                            downloadSettings.findPreference(
                                    DownloadSettings.PREF_LOCATION_PROMPT_ENABLED);
                    Assert.assertEquals(promptForDownload, locationPromptPreference.isChecked());
                    ManagedPreferenceDelegate delegate =
                            downloadSettings.getLocationPromptEnabledPrefDelegateForTesting();
                    Assert.assertTrue(
                            delegate.isPreferenceControlledByPolicy(locationPromptPreference));
                });
    }

    PrefService getPrefService() {
        return UserPrefs.get(ProfileManager.getLastUsedRegularProfile());
    }

    @Test
    @MediumTest
    public void testWithoutDownloadLater() throws Exception {
        mSettingsActivityTestRule.startSettingsActivity();
        assertPreference(DownloadSettings.PREF_LOCATION_CHANGE);
        assertPreference(DownloadSettings.PREF_LOCATION_PROMPT_ENABLED);
        mSettingsActivityTestRule.getActivity().finish();
    }

    @Test
    @MediumTest
    @Policies.Add({@Policies.Item(key = "PromptForDownloadLocation", string = "true")})
    public void testLocationPromptEnabledManagedByPolicy() throws Exception {
        waitForPolicyReady();
        mSettingsActivityTestRule.startSettingsActivity();
        verifyLocationPromptPolicy(true);
        mSettingsActivityTestRule.getActivity().finish();
    }

    @Test
    @MediumTest
    @Policies.Add({@Policies.Item(key = "PromptForDownloadLocation", string = "false")})
    public void testLocationPromptDisabledManagedByPolicy() throws Exception {
        waitForPolicyReady();
        mSettingsActivityTestRule.startSettingsActivity();
        verifyLocationPromptPolicy(false);
        mSettingsActivityTestRule.getActivity().finish();
    }
}
