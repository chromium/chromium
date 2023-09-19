// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safe_browsing.settings;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridge;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.TextMessagePreference;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for {@link StandardProtectionSettingsFragment}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "This test launches a Settings activity")
// clang-format off
public class StandardProtectionSettingsFragmentTest {
    // clang-format on
    private static final String ASSERT_MESSAGE_PREFIX = "Incorrect state: ";
    private static final String EXTENDED_REPORTING = "extended reporting ";
    private static final String LEAK_DETECTION = "leak detection ";
    private static final String ENABLED_STATE = "enabled state ";
    private static final String CHECKED_STATE = "checked state ";
    private static final String MANAGED_STATE = "managed state ";
    private static final String FROM_NATIVE = "from native";

    @Rule
    public SettingsActivityTestRule<StandardProtectionSettingsFragment> mTestRule =
            new SettingsActivityTestRule<>(StandardProtectionSettingsFragment.class);

    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    private TextMessagePreference mStandardProtectionSubtitle;
    private TextMessagePreference mStandardProtectionBulletOne;
    private TextMessagePreference mStandardProtectionBulletTwo;
    private ChromeSwitchPreference mExtendedReportingPreference;
    private ChromeSwitchPreference mPasswordLeakDetectionPreference;

    private void launchSettingsActivity() {
        mTestRule.startSettingsActivity();
        StandardProtectionSettingsFragment fragment = mTestRule.getFragment();
        mStandardProtectionSubtitle =
                fragment.findPreference(StandardProtectionSettingsFragment.PREF_SUBTITLE);
        mStandardProtectionBulletOne =
                fragment.findPreference(StandardProtectionSettingsFragment.PREF_BULLET_ONE);
        mStandardProtectionBulletTwo =
                fragment.findPreference(StandardProtectionSettingsFragment.PREF_BULLET_TWO);
        mExtendedReportingPreference =
                fragment.findPreference(StandardProtectionSettingsFragment.PREF_EXTENDED_REPORTING);
        mPasswordLeakDetectionPreference = fragment.findPreference(
                StandardProtectionSettingsFragment.PREF_PASSWORD_LEAK_DETECTION);
        Assert.assertNotNull(
                "Extended reporting preference should not be null.", mExtendedReportingPreference);
        Assert.assertNotNull("Password leak detection preference should not be null.",
                mPasswordLeakDetectionPreference);
    }

    @Test
    @SmallTest
    @Feature({"SafeBrowsing"})
    public void testSwitchExtendedReportingPreference() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SafeBrowsingBridge.setSafeBrowsingState(SafeBrowsingState.STANDARD_PROTECTION);
        });
        launchSettingsActivity();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            boolean is_extended_reporting_enabled =
                    SafeBrowsingBridge.isSafeBrowsingExtendedReportingEnabled();
            String checked_state_error_message =
                    ASSERT_MESSAGE_PREFIX + EXTENDED_REPORTING + CHECKED_STATE;
            String enabled_state_error_message =
                    ASSERT_MESSAGE_PREFIX + EXTENDED_REPORTING + ENABLED_STATE;
            Assert.assertEquals(checked_state_error_message, is_extended_reporting_enabled,
                    mExtendedReportingPreference.isChecked());
            Assert.assertTrue(
                    enabled_state_error_message, mExtendedReportingPreference.isEnabled());

            mExtendedReportingPreference.performClick();

            Assert.assertEquals(checked_state_error_message, !is_extended_reporting_enabled,
                    mExtendedReportingPreference.isChecked());
            Assert.assertEquals(enabled_state_error_message + FROM_NATIVE,
                    !is_extended_reporting_enabled,
                    SafeBrowsingBridge.isSafeBrowsingExtendedReportingEnabled());
        });
    }

    @Test
    @SmallTest
    @Feature({"SafeBrowsing"})
    @DisableFeatures(ChromeFeatureList.FRIENDLIER_SAFE_BROWSING_SETTINGS_STANDARD_PROTECTION)
    public void testSwitchPasswordLeakDetectionPreferenceOriginal() {
        mBrowserTestRule.addTestAccountThenSigninAndEnableSync();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SafeBrowsingBridge.setSafeBrowsingState(SafeBrowsingState.STANDARD_PROTECTION);
        });
        launchSettingsActivity();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            boolean is_password_leak_detection_enabled =
                    getPrefService().getBoolean(Pref.PASSWORD_LEAK_DETECTION_ENABLED);
            String checked_state_error_message =
                    ASSERT_MESSAGE_PREFIX + LEAK_DETECTION + CHECKED_STATE;
            String enabled_state_error_message =
                    ASSERT_MESSAGE_PREFIX + LEAK_DETECTION + ENABLED_STATE;
            Assert.assertEquals(checked_state_error_message, is_password_leak_detection_enabled,
                    mPasswordLeakDetectionPreference.isChecked());
            Assert.assertTrue(
                    enabled_state_error_message, mPasswordLeakDetectionPreference.isEnabled());
            Assert.assertNull("Leak detection summary should be null if there is an account.",
                    mPasswordLeakDetectionPreference.getSummary());

            mPasswordLeakDetectionPreference.performClick();

            Assert.assertEquals(checked_state_error_message, !is_password_leak_detection_enabled,
                    mPasswordLeakDetectionPreference.isChecked());
            Assert.assertEquals(enabled_state_error_message + FROM_NATIVE,
                    !is_password_leak_detection_enabled,
                    getPrefService().getBoolean(Pref.PASSWORD_LEAK_DETECTION_ENABLED));
        });
    }

    @Test
    @SmallTest
    @Feature({"SafeBrowsing"})
    @EnableFeatures(ChromeFeatureList.FRIENDLIER_SAFE_BROWSING_SETTINGS_STANDARD_PROTECTION)
    public void testSwitchPasswordLeakDetectionPreference() {
        mBrowserTestRule.addTestAccountThenSigninAndEnableSync();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SafeBrowsingBridge.setSafeBrowsingState(SafeBrowsingState.STANDARD_PROTECTION);
        });
        launchSettingsActivity();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            StandardProtectionSettingsFragment fragment = mTestRule.getFragment();
            boolean is_password_leak_detection_enabled =
                    getPrefService().getBoolean(Pref.PASSWORD_LEAK_DETECTION_ENABLED);
            String checked_state_error_message =
                    ASSERT_MESSAGE_PREFIX + LEAK_DETECTION + CHECKED_STATE;
            String enabled_state_error_message =
                    ASSERT_MESSAGE_PREFIX + LEAK_DETECTION + ENABLED_STATE;
            String password_leak_detection_summary = fragment.getContext().getString(
                    R.string.passwords_leak_detection_switch_summary);
            Assert.assertEquals(checked_state_error_message, is_password_leak_detection_enabled,
                    mPasswordLeakDetectionPreference.isChecked());
            Assert.assertTrue(
                    enabled_state_error_message, mPasswordLeakDetectionPreference.isEnabled());
            Assert.assertEquals(
                    password_leak_detection_summary, mPasswordLeakDetectionPreference.getSummary());

            mPasswordLeakDetectionPreference.performClick();

            Assert.assertEquals(checked_state_error_message, !is_password_leak_detection_enabled,
                    mPasswordLeakDetectionPreference.isChecked());
            Assert.assertEquals(enabled_state_error_message + FROM_NATIVE,
                    !is_password_leak_detection_enabled,
                    getPrefService().getBoolean(Pref.PASSWORD_LEAK_DETECTION_ENABLED));
        });
    }

    @Test
    @SmallTest
    @Feature({"SafeBrowsing"})
    @DisableFeatures(ChromeFeatureList.FRIENDLIER_SAFE_BROWSING_SETTINGS_STANDARD_PROTECTION)
    public void testPasswordLeakDetectionPreferenceEnabledForSignedOutUsersOriginal() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SafeBrowsingBridge.setSafeBrowsingState(SafeBrowsingState.STANDARD_PROTECTION);
        });
        launchSettingsActivity();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            boolean is_password_leak_detection_enabled =
                    getPrefService().getBoolean(Pref.PASSWORD_LEAK_DETECTION_ENABLED);
            String checked_state_error_message =
                    ASSERT_MESSAGE_PREFIX + LEAK_DETECTION + CHECKED_STATE;
            String enabled_state_error_message =
                    ASSERT_MESSAGE_PREFIX + LEAK_DETECTION + ENABLED_STATE;
            Assert.assertEquals(checked_state_error_message, is_password_leak_detection_enabled,
                    mPasswordLeakDetectionPreference.isChecked());
            Assert.assertTrue(
                    enabled_state_error_message, mPasswordLeakDetectionPreference.isEnabled());
            Assert.assertNull(
                    "Leak detection summary should be null if the unauthenticated leak detection "
                            + "is enabled.",
                    mPasswordLeakDetectionPreference.getSummary());

            mPasswordLeakDetectionPreference.performClick();

            Assert.assertEquals(checked_state_error_message, !is_password_leak_detection_enabled,
                    mPasswordLeakDetectionPreference.isChecked());
            Assert.assertEquals(enabled_state_error_message + FROM_NATIVE,
                    !is_password_leak_detection_enabled,
                    getPrefService().getBoolean(Pref.PASSWORD_LEAK_DETECTION_ENABLED));
        });
    }

    @Test
    @SmallTest
    @Feature({"SafeBrowsing"})
    @EnableFeatures(ChromeFeatureList.FRIENDLIER_SAFE_BROWSING_SETTINGS_STANDARD_PROTECTION)
    public void testPasswordLeakDetectionPreferenceEnabledForSignedOutUsers() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SafeBrowsingBridge.setSafeBrowsingState(SafeBrowsingState.STANDARD_PROTECTION);
        });
        launchSettingsActivity();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            StandardProtectionSettingsFragment fragment = mTestRule.getFragment();
            boolean is_password_leak_detection_enabled =
                    getPrefService().getBoolean(Pref.PASSWORD_LEAK_DETECTION_ENABLED);
            String checked_state_error_message =
                    ASSERT_MESSAGE_PREFIX + LEAK_DETECTION + CHECKED_STATE;
            String enabled_state_error_message =
                    ASSERT_MESSAGE_PREFIX + LEAK_DETECTION + ENABLED_STATE;
            String password_leak_detection_summary = fragment.getContext().getString(
                    R.string.passwords_leak_detection_switch_summary);
            Assert.assertEquals(checked_state_error_message, is_password_leak_detection_enabled,
                    mPasswordLeakDetectionPreference.isChecked());
            Assert.assertTrue(
                    enabled_state_error_message, mPasswordLeakDetectionPreference.isEnabled());
            Assert.assertEquals(
                    password_leak_detection_summary, mPasswordLeakDetectionPreference.getSummary());

            mPasswordLeakDetectionPreference.performClick();

            Assert.assertEquals(checked_state_error_message, !is_password_leak_detection_enabled,
                    mPasswordLeakDetectionPreference.isChecked());
            Assert.assertEquals(enabled_state_error_message + FROM_NATIVE,
                    !is_password_leak_detection_enabled,
                    getPrefService().getBoolean(Pref.PASSWORD_LEAK_DETECTION_ENABLED));
        });
    }

    @Test
    @SmallTest
    @Feature({"SafeBrowsing"})
    public void testPreferencesDisabledInEnhancedProtectionMode() {
        mBrowserTestRule.addTestAccountThenSigninAndEnableSync();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SafeBrowsingBridge.setSafeBrowsingState(SafeBrowsingState.ENHANCED_PROTECTION);
        });
        launchSettingsActivity();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse(ASSERT_MESSAGE_PREFIX + LEAK_DETECTION + ENABLED_STATE,
                    mPasswordLeakDetectionPreference.isEnabled());
            Assert.assertTrue(ASSERT_MESSAGE_PREFIX + LEAK_DETECTION + CHECKED_STATE,
                    mPasswordLeakDetectionPreference.isChecked());
            Assert.assertFalse(ASSERT_MESSAGE_PREFIX + EXTENDED_REPORTING + ENABLED_STATE,
                    mExtendedReportingPreference.isEnabled());
            Assert.assertTrue(ASSERT_MESSAGE_PREFIX + EXTENDED_REPORTING + CHECKED_STATE,
                    mExtendedReportingPreference.isChecked());
        });
    }

    @Test
    @SmallTest
    @Feature({"SafeBrowsing"})
    public void testPreferencesDisabledInNoProtectionMode() {
        mBrowserTestRule.addTestAccountThenSigninAndEnableSync();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SafeBrowsingBridge.setSafeBrowsingState(SafeBrowsingState.NO_SAFE_BROWSING);
        });
        launchSettingsActivity();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse(ASSERT_MESSAGE_PREFIX + LEAK_DETECTION + ENABLED_STATE,
                    mPasswordLeakDetectionPreference.isEnabled());
            Assert.assertFalse(ASSERT_MESSAGE_PREFIX + LEAK_DETECTION + CHECKED_STATE,
                    mPasswordLeakDetectionPreference.isChecked());
            Assert.assertFalse(ASSERT_MESSAGE_PREFIX + EXTENDED_REPORTING + ENABLED_STATE,
                    mExtendedReportingPreference.isEnabled());
            Assert.assertFalse(ASSERT_MESSAGE_PREFIX + EXTENDED_REPORTING + CHECKED_STATE,
                    mExtendedReportingPreference.isChecked());
        });
    }

    @Test
    @SmallTest
    @Feature({"SafeBrowsing"})
    @Policies.Add({ @Policies.Item(key = "PasswordLeakDetectionEnabled", string = "true") })
    public void testPasswordLeakDetectionPolicyManaged() {
        mBrowserTestRule.addTestAccountThenSigninAndEnableSync();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeBrowserInitializer.getInstance().handleSynchronousStartup();
            SafeBrowsingBridge.setSafeBrowsingState(SafeBrowsingState.STANDARD_PROTECTION);
        });
        launchSettingsActivity();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue(ASSERT_MESSAGE_PREFIX + LEAK_DETECTION + MANAGED_STATE + FROM_NATIVE,
                    getPrefService().isManagedPreference(Pref.PASSWORD_LEAK_DETECTION_ENABLED));
            Assert.assertFalse(ASSERT_MESSAGE_PREFIX + LEAK_DETECTION + ENABLED_STATE,
                    mPasswordLeakDetectionPreference.isEnabled());
            Assert.assertTrue(ASSERT_MESSAGE_PREFIX + LEAK_DETECTION + CHECKED_STATE,
                    mPasswordLeakDetectionPreference.isChecked());
        });
    }

    @Test
    @SmallTest
    @Feature({"SafeBrowsing"})
    @Policies.Add({ @Policies.Item(key = "SafeBrowsingExtendedReportingEnabled", string = "true") })
    public void testExtendedReportingPolicyManaged() {
        mBrowserTestRule.addTestAccountThenSigninAndEnableSync();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeBrowserInitializer.getInstance().handleSynchronousStartup();
            SafeBrowsingBridge.setSafeBrowsingState(SafeBrowsingState.STANDARD_PROTECTION);
        });
        launchSettingsActivity();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue(
                    ASSERT_MESSAGE_PREFIX + EXTENDED_REPORTING + MANAGED_STATE + FROM_NATIVE,
                    SafeBrowsingBridge.isSafeBrowsingExtendedReportingManaged());
            Assert.assertFalse(ASSERT_MESSAGE_PREFIX + EXTENDED_REPORTING + ENABLED_STATE,
                    mExtendedReportingPreference.isEnabled());
            Assert.assertTrue(ASSERT_MESSAGE_PREFIX + EXTENDED_REPORTING + CHECKED_STATE,
                    mExtendedReportingPreference.isChecked());
        });
    }

    @Test
    @SmallTest
    @Feature({"SafeBrowsing"})
    @EnableFeatures(ChromeFeatureList.FRIENDLIER_SAFE_BROWSING_SETTINGS_STANDARD_PROTECTION)
    public void testFriendlierSafeBrowsingSettingsStandardProtection() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SafeBrowsingBridge.setSafeBrowsingState(SafeBrowsingState.STANDARD_PROTECTION);
        });
        launchSettingsActivity();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Check that the bullet points have been removed
            Assert.assertNull(mStandardProtectionBulletOne);
            Assert.assertNull(mStandardProtectionBulletTwo);

            StandardProtectionSettingsFragment fragment = mTestRule.getFragment();

            String standardProtectionSubtitle = fragment.getContext().getString(
                    R.string.safe_browsing_standard_protection_subtitle_updated);
            String extended_reporting_title = fragment.getContext().getString(
                    R.string.safe_browsing_standard_protection_extended_reporting_title_updated);
            String password_leak_detection_title = fragment.getContext().getString(
                    R.string.passwords_leak_detection_switch_title_updated);
            String password_leak_detection_summary = fragment.getContext().getString(
                    R.string.passwords_leak_detection_switch_summary);

            Assert.assertEquals(standardProtectionSubtitle, mStandardProtectionSubtitle.getTitle());
            Assert.assertEquals(extended_reporting_title, mExtendedReportingPreference.getTitle());
            Assert.assertEquals(
                    password_leak_detection_title, mPasswordLeakDetectionPreference.getTitle());
            Assert.assertEquals(
                    password_leak_detection_summary, mPasswordLeakDetectionPreference.getSummary());
        });
    }

    @Test
    @SmallTest
    @Feature({"SafeBrowsing"})
    @DisableFeatures({ChromeFeatureList.FRIENDLIER_SAFE_BROWSING_SETTINGS_STANDARD_PROTECTION,
            ChromeFeatureList.HASH_PREFIX_REAL_TIME_LOOKUPS})
    public void
    testDisabledFriendlierSafeBrowsingSettingsStandardProtection() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SafeBrowsingBridge.setSafeBrowsingState(SafeBrowsingState.STANDARD_PROTECTION);
        });
        launchSettingsActivity();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Check that the bullet points are still here
            Assert.assertNotNull(mStandardProtectionBulletOne);
            Assert.assertNotNull(mStandardProtectionBulletTwo);

            StandardProtectionSettingsFragment fragment = mTestRule.getFragment();

            String standardProtectionSubtitle = fragment.getContext().getString(
                    R.string.safe_browsing_standard_protection_subtitle);
            String extended_reporting_title = fragment.getContext().getString(
                    R.string.safe_browsing_standard_protection_extended_reporting_title);
            String password_leak_detection_title =
                    fragment.getContext().getString(R.string.passwords_leak_detection_switch_title);
            String bulletTwoSummary = fragment.getContext().getString(
                    R.string.safe_browsing_standard_protection_bullet_two);

            Assert.assertEquals(standardProtectionSubtitle, mStandardProtectionSubtitle.getTitle());
            Assert.assertEquals(extended_reporting_title, mExtendedReportingPreference.getTitle());
            Assert.assertEquals(
                    password_leak_detection_title, mPasswordLeakDetectionPreference.getTitle());
            Assert.assertNull(mPasswordLeakDetectionPreference.getSummary());
            Assert.assertEquals(bulletTwoSummary, mStandardProtectionBulletTwo.getSummary());
        });
    }

    // TODO(crbug.com/1466292): Remove once friendlier safe browsing settings standard protection is
    // launched.
    @Test
    @SmallTest
    @Feature({"SafeBrowsing"})
    @EnableFeatures({ChromeFeatureList.HASH_PREFIX_REAL_TIME_LOOKUPS})
    @DisableFeatures({ChromeFeatureList.FRIENDLIER_SAFE_BROWSING_SETTINGS_STANDARD_PROTECTION})
    public void testDisabledFriendlierSafeBrowsingSettingsStandardProtectionWithProxy() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SafeBrowsingBridge.setSafeBrowsingState(SafeBrowsingState.STANDARD_PROTECTION);
        });
        launchSettingsActivity();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            StandardProtectionSettingsFragment fragment = mTestRule.getFragment();
            String bulletTwoSummary = fragment.getContext().getString(
                    R.string.safe_browsing_standard_protection_bullet_two_proxy);
            if (!BuildConfig.IS_CHROME_BRANDED) {
                // HPRT is disabled on Chromium build.
                bulletTwoSummary = fragment.getContext().getString(
                        R.string.safe_browsing_standard_protection_bullet_two);
            }
            Assert.assertEquals(bulletTwoSummary, mStandardProtectionBulletTwo.getSummary());
        });
    }

    PrefService getPrefService() {
        return UserPrefs.get(Profile.getLastUsedRegularProfile());
    }
}
