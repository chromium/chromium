// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safe_browsing.settings;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridge;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.TextMessagePreference;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.components.user_prefs.UserPrefs;

/** Tests for {@link StandardProtectionSettingsFragment}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "This test launches a Settings activity")
public class StandardProtectionSettingsFragmentTest {
    private static final String ASSERT_MESSAGE_PREFIX = "Incorrect state: ";
    private static final String EXTENDED_REPORTING = "extended reporting ";
    private static final String ENABLED_STATE = "enabled state ";
    private static final String CHECKED_STATE = "checked state ";
    private static final String MANAGED_STATE = "managed state ";
    private static final String FROM_NATIVE = "from native";

    @Rule
    public SettingsActivityTestRule<StandardProtectionSettingsFragment> mTestRule =
            new SettingsActivityTestRule<>(StandardProtectionSettingsFragment.class);

    @Rule public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    private ChromeSwitchPreference mExtendedReportingPreference;
    private TextMessagePreference mStandardProtectionSubtitle;

    // TODO(crbug.com/336547987): Add a new test for checking that mExtendedReportingPreference is
    // not shown when the flag is enabled.
    private void startSettings() {
        mTestRule.startSettingsActivity();
        StandardProtectionSettingsFragment fragment = mTestRule.getFragment();
        mExtendedReportingPreference =
                fragment.findPreference(StandardProtectionSettingsFragment.PREF_EXTENDED_REPORTING);
        mStandardProtectionSubtitle =
                fragment.findPreference(StandardProtectionSettingsFragment.PREF_SUBTITLE);
        Assert.assertNotNull(
                "Extended reporting preference should not be null.", mExtendedReportingPreference);
    }

    private void setSafeBrowsingState(@SafeBrowsingState int state) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    new SafeBrowsingBridge(ProfileManager.getLastUsedRegularProfile())
                            .setSafeBrowsingState(state);
                });
    }

    private boolean isSafeBrowsingExtendedReportingEnabled() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    return new SafeBrowsingBridge(ProfileManager.getLastUsedRegularProfile())
                            .isSafeBrowsingExtendedReportingEnabled();
                });
    }

    @Test
    @SmallTest
    @Feature({"SafeBrowsing"})
    @DisableFeatures({ChromeFeatureList.SAFE_BROWSING_EXTENDED_REPORTING_REMOVE_PREF_DEPENDENCY})
    public void testSwitchExtendedReportingPreference() {
        setSafeBrowsingState(SafeBrowsingState.STANDARD_PROTECTION);
        startSettings();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    boolean is_extended_reporting_enabled =
                            isSafeBrowsingExtendedReportingEnabled();
                    String checked_state_error_message =
                            ASSERT_MESSAGE_PREFIX + EXTENDED_REPORTING + CHECKED_STATE;
                    String enabled_state_error_message =
                            ASSERT_MESSAGE_PREFIX + EXTENDED_REPORTING + ENABLED_STATE;
                    Assert.assertEquals(
                            checked_state_error_message,
                            is_extended_reporting_enabled,
                            mExtendedReportingPreference.isChecked());
                    Assert.assertTrue(
                            enabled_state_error_message, mExtendedReportingPreference.isEnabled());

                    mExtendedReportingPreference.performClick();

                    Assert.assertEquals(
                            checked_state_error_message,
                            !is_extended_reporting_enabled,
                            mExtendedReportingPreference.isChecked());
                    Assert.assertEquals(
                            enabled_state_error_message + FROM_NATIVE,
                            !is_extended_reporting_enabled,
                            isSafeBrowsingExtendedReportingEnabled());
                });
    }

    @Test
    @SmallTest
    @Feature({"SafeBrowsing"})
    public void testPreferenceDisabledInEnhancedProtectionMode() {
        mBrowserTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        setSafeBrowsingState(SafeBrowsingState.ENHANCED_PROTECTION);
        startSettings();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertFalse(
                            ASSERT_MESSAGE_PREFIX + EXTENDED_REPORTING + ENABLED_STATE,
                            mExtendedReportingPreference.isEnabled());
                    Assert.assertTrue(
                            ASSERT_MESSAGE_PREFIX + EXTENDED_REPORTING + CHECKED_STATE,
                            mExtendedReportingPreference.isChecked());
                });
    }

    @Test
    @SmallTest
    @Feature({"SafeBrowsing"})
    public void testPreferenceDisabledInNoProtectionMode() {
        mBrowserTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        setSafeBrowsingState(SafeBrowsingState.NO_SAFE_BROWSING);
        startSettings();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertFalse(
                            ASSERT_MESSAGE_PREFIX + EXTENDED_REPORTING + ENABLED_STATE,
                            mExtendedReportingPreference.isEnabled());
                    Assert.assertFalse(
                            ASSERT_MESSAGE_PREFIX + EXTENDED_REPORTING + CHECKED_STATE,
                            mExtendedReportingPreference.isChecked());
                });
    }

    @Test
    @SmallTest
    @Feature({"SafeBrowsing"})
    @DisableFeatures({ChromeFeatureList.SAFE_BROWSING_EXTENDED_REPORTING_REMOVE_PREF_DEPENDENCY})
    @Policies.Add({@Policies.Item(key = "SafeBrowsingExtendedReportingEnabled", string = "true")})
    public void testExtendedReportingPolicyManaged() {
        mBrowserTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeBrowserInitializer.getInstance().handleSynchronousStartup();
                    setSafeBrowsingState(SafeBrowsingState.STANDARD_PROTECTION);
                });
        startSettings();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(
                            ASSERT_MESSAGE_PREFIX
                                    + EXTENDED_REPORTING
                                    + MANAGED_STATE
                                    + FROM_NATIVE,
                            new SafeBrowsingBridge(ProfileManager.getLastUsedRegularProfile())
                                    .isSafeBrowsingExtendedReportingManaged());
                    Assert.assertFalse(
                            ASSERT_MESSAGE_PREFIX + EXTENDED_REPORTING + ENABLED_STATE,
                            mExtendedReportingPreference.isEnabled());
                    Assert.assertTrue(
                            ASSERT_MESSAGE_PREFIX + EXTENDED_REPORTING + CHECKED_STATE,
                            mExtendedReportingPreference.isChecked());
                });
    }

    @Test
    @SmallTest
    @Feature({"SafeBrowsing"})
    public void testSafeBrowsingSettingsStandardProtection() {
        setSafeBrowsingState(SafeBrowsingState.STANDARD_PROTECTION);
        startSettings();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    StandardProtectionSettingsFragment fragment = mTestRule.getFragment();

                    String standardProtectionSubtitle =
                            fragment.getContext()
                                    .getString(R.string.safe_browsing_standard_protection_subtitle);
                    String extended_reporting_title =
                            fragment.getContext()
                                    .getString(
                                            R.string
                                                    .safe_browsing_standard_protection_extended_reporting_title);

                    Assert.assertEquals(
                            standardProtectionSubtitle, mStandardProtectionSubtitle.getTitle());
                    Assert.assertEquals(
                            extended_reporting_title, mExtendedReportingPreference.getTitle());
                });
    }

    PrefService getPrefService() {
        return UserPrefs.get(ProfileManager.getLastUsedRegularProfile());
    }
}
