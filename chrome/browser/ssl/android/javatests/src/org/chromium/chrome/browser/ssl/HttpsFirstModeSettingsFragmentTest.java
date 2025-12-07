// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ssl;

import androidx.preference.Preference;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.AdvancedProtectionTestRule;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescriptionLayout;
import org.chromium.components.policy.test.annotations.Policies;

/** Tests for {@link HttpsFirstModeSettingsFragment}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "This test launches a Settings activity")
@EnableFeatures(ChromeFeatureList.HTTPS_FIRST_BALANCED_MODE)
public class HttpsFirstModeSettingsFragmentTest {
    private static final String ASSERT_HFM_STATE_RADIO_BUTTON_GROUP =
            "Incorrect HFM state in the radio button group.";
    private static final String ASSERT_SWITCH_ENABLED = "Incorrect switch enabled state.";
    private static final String ASSERT_RADIO_BUTTON_CHECKED =
            "Incorrect radio button checked state.";
    private static final String ASSERT_HFM_STATE_NATIVE = "Incorrect HFM state from native.";

    @ClassRule
    public static AdvancedProtectionTestRule sAdvancedProtectionRule =
            new AdvancedProtectionTestRule();

    @Rule
    public SettingsActivityTestRule<HttpsFirstModeSettingsFragment> mSettingsTestRule =
            new SettingsActivityTestRule<>(HttpsFirstModeSettingsFragment.class);

    @Before
    public void setUp() {
        sAdvancedProtectionRule.setIsAdvancedProtectionRequestedByOs(false);
    }

    private HttpsFirstModeSettingsFragment mHttpsFirstModeSettingsFragment;
    private ChromeSwitchPreference mHttpsFirstModeTogglePref;
    private HttpsFirstModeVariantPreference mHttpsFirstModeVariantPref;
    private RadioButtonWithDescriptionLayout mHttpsFirstModeVariantGroup;
    private RadioButtonWithDescription mHttpsFirstModeVariantStrict;
    private RadioButtonWithDescription mHttpsFirstModeVariantBalanced;
    private Preference mEnforcedByAdvancedProtectionWarning;

    private void startSettings() {
        mSettingsTestRule.startSettingsActivity();
        mHttpsFirstModeSettingsFragment = mSettingsTestRule.getFragment();
        mHttpsFirstModeTogglePref =
                mHttpsFirstModeSettingsFragment.findPreference(
                        HttpsFirstModeSettingsFragment.PREF_HTTPS_FIRST_MODE_SWITCH);
        mHttpsFirstModeVariantPref =
                mHttpsFirstModeSettingsFragment.findPreference(
                        HttpsFirstModeSettingsFragment.PREF_HTTPS_FIRST_MODE_VARIANT);
        Assert.assertNotNull("Switch preference should not be null.", mHttpsFirstModeTogglePref);
        Assert.assertNotNull("Variant preference should not be null.", mHttpsFirstModeVariantPref);

        mEnforcedByAdvancedProtectionWarning =
                mHttpsFirstModeSettingsFragment.findPreference(
                        HttpsFirstModeSettingsFragment.PREF_ENFORCED_BY_ADVANCED_PROTECTION);
    }

    @HttpsFirstModeSetting
    private int getHttpsFirstModeState() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    return new HttpsFirstModeBridge(ProfileManager.getLastUsedRegularProfile())
                            .getCurrentSetting();
                });
    }

    private boolean isSettingManaged() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    return new HttpsFirstModeBridge(ProfileManager.getLastUsedRegularProfile())
                            .isManaged();
                });
    }

    @Test
    @SmallTest
    @Feature({"HttpsFirstMode"})
    public void testOnStartup() {
        startSettings();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    @HttpsFirstModeSetting int currentState = getHttpsFirstModeState();
                    boolean strict_mode_checked =
                            currentState == HttpsFirstModeSetting.ENABLED_FULL;
                    boolean balanced_mode_checked =
                            (currentState == HttpsFirstModeSetting.ENABLED_BALANCED
                                    || currentState == HttpsFirstModeSetting.DISABLED);
                    boolean toggle_on = currentState != HttpsFirstModeSetting.DISABLED;
                    Assert.assertEquals(
                            ASSERT_RADIO_BUTTON_CHECKED,
                            strict_mode_checked,
                            getStrictModeButton().isChecked());
                    Assert.assertEquals(
                            ASSERT_RADIO_BUTTON_CHECKED,
                            balanced_mode_checked,
                            getBalancedModeButton().isChecked());
                    Assert.assertEquals(
                            ASSERT_RADIO_BUTTON_CHECKED,
                            toggle_on,
                            mHttpsFirstModeTogglePref.isChecked());
                    // Variant pref is enabled if toggle is on.
                    Assert.assertEquals(
                            "Incorrect variant pref enabled state.",
                            toggle_on,
                            mHttpsFirstModeVariantPref.isEnabled());
                });
    }

    @Test
    @SmallTest
    @Feature({"HttpsFirstMode"})
    @EnableFeatures("HttpsFirstBalancedModeAutoEnable")
    public void testPreferenceControls() {
        startSettings();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Toggle enabled and balanced mode selected by default.
                    Assert.assertTrue(ASSERT_SWITCH_ENABLED, mHttpsFirstModeTogglePref.isChecked());
                    Assert.assertTrue(
                            "Variant pref has incorrect enabled state.",
                            mHttpsFirstModeVariantPref.isEnabled());
                    Assert.assertFalse(
                            ASSERT_RADIO_BUTTON_CHECKED, getStrictModeButton().isChecked());
                    Assert.assertTrue(
                            ASSERT_RADIO_BUTTON_CHECKED, getBalancedModeButton().isChecked());
                    Assert.assertEquals(
                            ASSERT_HFM_STATE_NATIVE,
                            HttpsFirstModeSetting.ENABLED_BALANCED,
                            getHttpsFirstModeState());

                    // Click the Strict Mode button.
                    getStrictModeButton().onClick(null);
                    Assert.assertTrue(ASSERT_SWITCH_ENABLED, mHttpsFirstModeTogglePref.isChecked());
                    Assert.assertTrue(
                            ASSERT_RADIO_BUTTON_CHECKED, getStrictModeButton().isChecked());
                    Assert.assertFalse(
                            ASSERT_RADIO_BUTTON_CHECKED, getBalancedModeButton().isChecked());
                    Assert.assertEquals(
                            ASSERT_HFM_STATE_NATIVE,
                            HttpsFirstModeSetting.ENABLED_FULL,
                            getHttpsFirstModeState());

                    // Click the Balanced Mode button.
                    getBalancedModeButton().onClick(null);
                    Assert.assertTrue(ASSERT_SWITCH_ENABLED, mHttpsFirstModeTogglePref.isChecked());
                    Assert.assertFalse(
                            ASSERT_RADIO_BUTTON_CHECKED, getStrictModeButton().isChecked());
                    Assert.assertTrue(
                            ASSERT_RADIO_BUTTON_CHECKED, getBalancedModeButton().isChecked());
                    Assert.assertEquals(
                            ASSERT_HFM_STATE_NATIVE,
                            HttpsFirstModeSetting.ENABLED_BALANCED,
                            getHttpsFirstModeState());

                    // Flip the main toggle off.
                    mHttpsFirstModeTogglePref.onClick();
                    Assert.assertFalse(mHttpsFirstModeVariantPref.isEnabled());
                    Assert.assertFalse(
                            ASSERT_SWITCH_ENABLED, mHttpsFirstModeTogglePref.isChecked());
                    Assert.assertFalse(
                            ASSERT_RADIO_BUTTON_CHECKED, getStrictModeButton().isChecked());
                    // When the main toggle is off, the "Balanced Mode" radio button should
                    // remain checked (but the Preference control is disabled).
                    Assert.assertTrue(
                            ASSERT_RADIO_BUTTON_CHECKED, getBalancedModeButton().isChecked());
                    Assert.assertEquals(
                            ASSERT_HFM_STATE_NATIVE,
                            HttpsFirstModeSetting.DISABLED,
                            getHttpsFirstModeState());
                });
    }

    @Test
    @SmallTest
    @Feature({"HttpsFirstMode"})
    public void testSetting_AdvancedProtectionEnabled() {
        sAdvancedProtectionRule.setIsAdvancedProtectionRequestedByOs(true);

        startSettings();
        final String lockedSummaryText =
                ApplicationProvider.getApplicationContext()
                        .getString(
                                R.string
                                        .settings_https_first_mode_with_advanced_protection_description);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // A custom summary should be shown for APP users.
                    Assert.assertEquals(
                            "Incorrect summary string for APP mode user.",
                            lockedSummaryText,
                            mHttpsFirstModeTogglePref.getSummary());

                    // Strict mode should be set.
                    Assert.assertTrue(ASSERT_SWITCH_ENABLED, mHttpsFirstModeTogglePref.isChecked());
                    Assert.assertTrue(
                            ASSERT_RADIO_BUTTON_CHECKED, getStrictModeButton().isChecked());
                    Assert.assertFalse(
                            ASSERT_RADIO_BUTTON_CHECKED, getBalancedModeButton().isChecked());
                    Assert.assertEquals(
                            ASSERT_HFM_STATE_NATIVE,
                            HttpsFirstModeSetting.ENABLED_FULL,
                            getHttpsFirstModeState());

                    // Toggle and variant preferences should be disabled.
                    Assert.assertFalse(
                            "Preference not correctly disabled.",
                            mHttpsFirstModeTogglePref.isEnabled());
                    Assert.assertFalse(
                            "Preference not correctly disabled.",
                            mHttpsFirstModeVariantPref.isEnabled());

                    Assert.assertNotNull(mEnforcedByAdvancedProtectionWarning);
                });
    }

    @Test
    @SmallTest
    @Feature({"HttpsFirstMode"})
    public void testSetting_AdvancedProtectionDisabled() {
        sAdvancedProtectionRule.setIsAdvancedProtectionRequestedByOs(false);

        startSettings();
        Assert.assertNull(mEnforcedByAdvancedProtectionWarning);
    }

    @Test
    @SmallTest
    @Feature({"HttpsFirstMode"})
    @Policies.Add({@Policies.Item(key = "HttpsOnlyMode", string = "disallowed")})
    public void testHttpsFirstModeManagedDisallowed() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeBrowserInitializer.getInstance().handleSynchronousStartup();
                });
        startSettings();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(isSettingManaged());
                    Assert.assertFalse(mHttpsFirstModeTogglePref.isEnabled());
                    Assert.assertEquals(HttpsFirstModeSetting.DISABLED, getHttpsFirstModeState());

                    // Radio group should be hidden when the settings is force-disabled by policy.
                    Assert.assertFalse(mHttpsFirstModeVariantPref.isVisible());
                });
    }

    @Test
    @SmallTest
    @Feature({"HttpsFirstMode"})
    @Policies.Add({@Policies.Item(key = "HttpsOnlyMode", string = "force_enabled")})
    public void testHttpsFirstModeManagedForceEnabled() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeBrowserInitializer.getInstance().handleSynchronousStartup();
                });
        startSettings();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(isSettingManaged());
                    Assert.assertFalse(getStrictModeButton().isEnabled());
                    Assert.assertTrue(getStrictModeButton().isChecked());
                    Assert.assertFalse(getBalancedModeButton().isEnabled());
                    Assert.assertFalse(mHttpsFirstModeTogglePref.isEnabled());
                    Assert.assertTrue(mHttpsFirstModeTogglePref.isChecked());
                    Assert.assertTrue(mHttpsFirstModeVariantPref.isVisible());
                    Assert.assertEquals(
                            HttpsFirstModeSetting.ENABLED_FULL, getHttpsFirstModeState());
                });
    }

    @Test
    @SmallTest
    @Feature({"HttpsFirstMode"})
    @Policies.Add({@Policies.Item(key = "HttpsOnlyMode", string = "force_balanced_enabled")})
    public void testHttpsFirstModeManagedForceBalancedEnabled() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeBrowserInitializer.getInstance().handleSynchronousStartup();
                });
        startSettings();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(isSettingManaged());
                    Assert.assertFalse(getStrictModeButton().isEnabled());
                    Assert.assertFalse(getBalancedModeButton().isEnabled());
                    Assert.assertTrue(getBalancedModeButton().isChecked());
                    Assert.assertFalse(mHttpsFirstModeTogglePref.isEnabled());
                    Assert.assertTrue(mHttpsFirstModeTogglePref.isChecked());
                    Assert.assertTrue(mHttpsFirstModeVariantPref.isVisible());
                    Assert.assertEquals(
                            HttpsFirstModeSetting.ENABLED_BALANCED, getHttpsFirstModeState());
                });
    }

    private RadioButtonWithDescription getStrictModeButton() {
        return mHttpsFirstModeVariantPref.getStrictModeButtonForTesting();
    }

    private RadioButtonWithDescription getBalancedModeButton() {
        return mHttpsFirstModeVariantPref.getBalancedModeButtonForTesting();
    }
}
