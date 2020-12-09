// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.test.filters.LargeTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for AutofillAssistantPreferenceFragment.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantPreferenceFragmentTest {
    private final SettingsActivityTestRule<AutofillAssistantPreferenceFragment>
            mSettingsActivityTestRule =
                    new SettingsActivityTestRule<>(AutofillAssistantPreferenceFragment.class);

    private final SharedPreferencesManager mSharedPreferencesManager =
            SharedPreferencesManager.getInstance();

    /**
     * Test: if the onboarding was never shown or it was shown and not accepted, the AA chrome
     * preference should not exist.
     *
     * Note: the presence of the {@link AutofillAssistantPreferenceFragment.PREF_AUTOFILL_ASSISTANT}
     * shared preference indicates whether the onboarding was accepted or not.
     */
    @Test
    @LargeTest
    @Feature({"Sync"})
    @EnableFeatures(ChromeFeatureList.AUTOFILL_ASSISTANT)
    public void testAutofillAssistantNoPreferenceIfOnboardingNeverShown() {
        final AutofillAssistantPreferenceFragment prefs =
                startAutofillAssistantPreferenceFragment();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertFalse(prefs.findPreference(
                                     AutofillAssistantPreferenceFragment.PREF_AUTOFILL_ASSISTANT)
                                .isVisible());
        });
    }

    /**
     * Test: if the onboarding was accepted, the AA chrome preference should also exist.
     *
     * Note: the presence of the {@link AutofillAssistantPreferenceFragment.PREF_AUTOFILL_ASSISTANT}
     * shared preference indicates whether the onboarding was accepted or not.
     */
    @Test
    @LargeTest
    @Feature({"Sync"})
    @EnableFeatures(ChromeFeatureList.AUTOFILL_ASSISTANT)
    public void testAutofillAssistantPreferenceShownIfOnboardingShown() {
        setAutofillAssistantSwitchValue(true);
        final AutofillAssistantPreferenceFragment prefs =
                startAutofillAssistantPreferenceFragment();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertTrue(prefs.findPreference(
                                    AutofillAssistantPreferenceFragment.PREF_AUTOFILL_ASSISTANT)
                               .isVisible());
        });
    }

    /**
     * Ensure that the "Autofill Assistant" setting is not shown when the feature is disabled.
     */
    @Test
    @LargeTest
    @Feature({"Sync"})
    @DisableFeatures(ChromeFeatureList.AUTOFILL_ASSISTANT)
    public void testAutofillAssistantNoPreferenceIfFeatureDisabled() {
        setAutofillAssistantSwitchValue(true);
        final AutofillAssistantPreferenceFragment prefs =
                startAutofillAssistantPreferenceFragment();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertFalse(prefs.findPreference(
                                     AutofillAssistantPreferenceFragment.PREF_AUTOFILL_ASSISTANT)
                                .isVisible());
        });
    }

    /**
     * Ensure that the "Autofill Assistant" on/off switch works.
     */
    @Test
    @LargeTest
    @Feature({"Sync"})
    @EnableFeatures(ChromeFeatureList.AUTOFILL_ASSISTANT)
    public void testAutofillAssistantSwitchOn() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { setAutofillAssistantSwitchValue(true); });
        final AutofillAssistantPreferenceFragment prefs =
                startAutofillAssistantPreferenceFragment();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeSwitchPreference autofillAssistantSwitch =
                    (ChromeSwitchPreference) prefs.findPreference(
                            AutofillAssistantPreferenceFragment.PREF_AUTOFILL_ASSISTANT);
            assertTrue(autofillAssistantSwitch.isChecked());

            autofillAssistantSwitch.performClick();
            assertFalse(mSharedPreferencesManager.readBoolean(
                    ChromePreferenceKeys.AUTOFILL_ASSISTANT_ENABLED, true));
            autofillAssistantSwitch.performClick();
            assertTrue(mSharedPreferencesManager.readBoolean(
                    ChromePreferenceKeys.AUTOFILL_ASSISTANT_ENABLED, false));
        });
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ASSISTANT,
            ChromeFeatureList.AUTOFILL_ASSISTANT_PROACTIVE_HELP})
    @DisableFeatures(ChromeFeatureList.AUTOFILL_ASSISTANT_DISABLE_PROACTIVE_HELP_TIED_TO_MSBB)
    public void
    testProactiveHelpDisabledIfMsbbDisabled() {
        final AutofillAssistantPreferenceFragment prefs =
                startAutofillAssistantPreferenceFragment();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeSwitchPreference proactiveHelpSwitch =
                    (ChromeSwitchPreference) prefs.findPreference(
                            AutofillAssistantPreferenceFragment
                                    .PREF_ASSISTANT_PROACTIVE_HELP_SWITCH);
            assertFalse(proactiveHelpSwitch.isEnabled());

            Preference syncAndServicesLink = prefs.findPreference(
                    AutofillAssistantPreferenceFragment.PREF_GOOGLE_SERVICES_SETTINGS_LINK);
            assertNotNull(syncAndServicesLink);
            assertTrue(syncAndServicesLink.isVisible());
        });
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ASSISTANT,
            ChromeFeatureList.AUTOFILL_ASSISTANT_PROACTIVE_HELP,
            ChromeFeatureList.AUTOFILL_ASSISTANT_DISABLE_PROACTIVE_HELP_TIED_TO_MSBB})
    public void
    testProactiveHelpNotLinkedToMsbbIfLinkDisabled() {
        final AutofillAssistantPreferenceFragment prefs =
                startAutofillAssistantPreferenceFragment();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeSwitchPreference proactiveHelpSwitch =
                    (ChromeSwitchPreference) prefs.findPreference(
                            AutofillAssistantPreferenceFragment
                                    .PREF_ASSISTANT_PROACTIVE_HELP_SWITCH);
            assertTrue(proactiveHelpSwitch.isEnabled());

            Preference syncAndServicesLink = prefs.findPreference(
                    AutofillAssistantPreferenceFragment.PREF_GOOGLE_SERVICES_SETTINGS_LINK);
            assertNotNull(syncAndServicesLink);
            assertFalse(syncAndServicesLink.isVisible());
        });
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ASSISTANT,
            ChromeFeatureList.AUTOFILL_ASSISTANT_PROACTIVE_HELP})
    @DisableFeatures(ChromeFeatureList.AUTOFILL_ASSISTANT_DISABLE_PROACTIVE_HELP_TIED_TO_MSBB)
    public void
    testProactiveHelpDisabledIfAutofillAssistantDisabled() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { setAutofillAssistantSwitchValue(true); });
        final AutofillAssistantPreferenceFragment prefs =
                startAutofillAssistantPreferenceFragment();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeSwitchPreference proactiveHelpSwitch =
                    (ChromeSwitchPreference) prefs.findPreference(
                            AutofillAssistantPreferenceFragment
                                    .PREF_ASSISTANT_PROACTIVE_HELP_SWITCH);
            Preference syncAndServicesLink = prefs.findPreference(
                    AutofillAssistantPreferenceFragment.PREF_GOOGLE_SERVICES_SETTINGS_LINK);
            assertFalse(proactiveHelpSwitch.isEnabled());

            assertTrue(syncAndServicesLink.isVisible());

            ChromeSwitchPreference autofillAssistantSwitch =
                    (ChromeSwitchPreference) prefs.findPreference(
                            AutofillAssistantPreferenceFragment.PREF_AUTOFILL_ASSISTANT);
            autofillAssistantSwitch.performClick();

            assertFalse(proactiveHelpSwitch.isEnabled());
            assertFalse(syncAndServicesLink.isVisible());
        });
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    @DisableFeatures(ChromeFeatureList.AUTOFILL_ASSISTANT_PROACTIVE_HELP)
    public void testProactiveHelpInvisibleIfProactiveHelpDisabled() {
        final AutofillAssistantPreferenceFragment prefs =
                startAutofillAssistantPreferenceFragment();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeSwitchPreference proactiveHelpSwitch =
                    (ChromeSwitchPreference) prefs.findPreference(
                            AutofillAssistantPreferenceFragment
                                    .PREF_ASSISTANT_PROACTIVE_HELP_SWITCH);
            assertFalse(proactiveHelpSwitch.isVisible());
        });
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ASSISTANT,
            ChromeFeatureList.AUTOFILL_ASSISTANT_PROACTIVE_HELP})
    public void
    testWebAssistanceInvisibleIfAutofillAssistantCompletelyDisabled() {
        final AutofillAssistantPreferenceFragment prefs =
                startAutofillAssistantPreferenceFragment();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PreferenceCategory webAssistanceCateogory = prefs.findPreference(
                    AutofillAssistantPreferenceFragment.PREF_WEB_ASSISTANCE_CATEGORY);
            assertFalse(webAssistanceCateogory.isVisible());
        });
    }

    @Test
    @LargeTest
    @Feature({"AssistantVoiceSearch"})
    @EnableFeatures(ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH)
    public void testEnhancedVoiceSearch_Enabled() {
        final AutofillAssistantPreferenceFragment prefs =
                startAutofillAssistantPreferenceFragment();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PreferenceCategory assistantVoiceSearchCategory = prefs.findPreference(
                    AutofillAssistantPreferenceFragment.PREF_ASSISTANT_VOICE_SEARCH_CATEGORY);
            assertTrue(assistantVoiceSearchCategory.isVisible());

            ChromeSwitchPreference assistantVoiceSearchEnabledSwitch =
                    (ChromeSwitchPreference) prefs.findPreference(
                            AutofillAssistantPreferenceFragment
                                    .PREF_ASSISTANT_VOICE_SEARCH_ENABLED_SWTICH);
            assertTrue(assistantVoiceSearchEnabledSwitch.isVisible());
            assistantVoiceSearchEnabledSwitch.performClick();
            assertTrue(mSharedPreferencesManager.readBoolean(
                    ChromePreferenceKeys.ASSISTANT_VOICE_SEARCH_ENABLED, /* default= */ false));
        });
    }

    @Test
    @LargeTest
    @Feature({"AssistantVoiceSearch"})
    @DisableFeatures(ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH)
    public void testEnhancedVoiceSearch_Disabled() {
        final AutofillAssistantPreferenceFragment prefs =
                startAutofillAssistantPreferenceFragment();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PreferenceCategory assistantVoiceSearchCategory = prefs.findPreference(
                    AutofillAssistantPreferenceFragment.PREF_ASSISTANT_VOICE_SEARCH_CATEGORY);
            assertFalse(assistantVoiceSearchCategory.isVisible());

            ChromeSwitchPreference assistantVoiceSearchEnabledSwitch =
                    (ChromeSwitchPreference) prefs.findPreference(
                            AutofillAssistantPreferenceFragment
                                    .PREF_ASSISTANT_VOICE_SEARCH_ENABLED_SWTICH);
            assertFalse(assistantVoiceSearchEnabledSwitch.isVisible());
        });
    }

    private void setAutofillAssistantSwitchValue(boolean newValue) {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.AUTOFILL_ASSISTANT_ENABLED, newValue);
    }

    private AutofillAssistantPreferenceFragment startAutofillAssistantPreferenceFragment() {
        mSettingsActivityTestRule.startSettingsActivity();
        return mSettingsActivityTestRule.getFragment();
    }

    public boolean isAutofillAssistantSwitchOn() {
        return mSharedPreferencesManager.readBoolean(
                ChromePreferenceKeys.AUTOFILL_ASSISTANT_ENABLED, false);
    }
}
