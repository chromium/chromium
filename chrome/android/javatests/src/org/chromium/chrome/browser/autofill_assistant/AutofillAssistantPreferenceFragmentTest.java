// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.test.filters.LargeTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.autofill_assistant.AssistantFeatures;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for AutofillAssistantPreferenceFragment.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AutofillAssistantPreferenceFragmentTest {
    private final SharedPreferencesManager mSharedPreferencesManager =
            SharedPreferencesManager.getInstance();

    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    public final SettingsActivityTestRule<AutofillAssistantPreferenceFragment>
            mSettingsActivityTestRule =
                    new SettingsActivityTestRule<>(AutofillAssistantPreferenceFragment.class);

    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mActivityTestRule).around(mSettingsActivityTestRule);

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Ensure that no state was leaked from another test.
            PrefService prefService = UserPrefs.get(Profile.getLastUsedRegularProfile());
            prefService.clearPref(Pref.AUTOFILL_ASSISTANT_CONSENT);
            prefService.clearPref(Pref.AUTOFILL_ASSISTANT_ENABLED);
            prefService.clearPref(Pref.AUTOFILL_ASSISTANT_TRIGGER_SCRIPTS_ENABLED);
        });
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Ensure that no state leaks into another test.
            PrefService prefService = UserPrefs.get(Profile.getLastUsedRegularProfile());
            prefService.clearPref(Pref.AUTOFILL_ASSISTANT_CONSENT);
            prefService.clearPref(Pref.AUTOFILL_ASSISTANT_ENABLED);
            prefService.clearPref(Pref.AUTOFILL_ASSISTANT_TRIGGER_SCRIPTS_ENABLED);
        });
    }

    /** Returns the value of @param preference. Must be called on the UI thread. */
    private boolean getBooleanPref(String preference) {
        PrefService prefService = UserPrefs.get(Profile.getLastUsedRegularProfile());
        return prefService.getBoolean(preference);
    }

    /** Simulates accepted Autofill Assistant onboarding by setting the relevant prefs. */
    private void acceptAssistantOnboarding() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PrefService prefService = UserPrefs.get(Profile.getLastUsedRegularProfile());
            prefService.setBoolean(Pref.AUTOFILL_ASSISTANT_CONSENT, true);
            prefService.setBoolean(Pref.AUTOFILL_ASSISTANT_ENABLED, true);
        });
    }

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
    @EnableFeatures(AssistantFeatures.AUTOFILL_ASSISTANT_NAME)
    public void testAutofillAssistantNoPreferenceIfOnboardingNeverShown() {
        final AutofillAssistantPreferenceFragment prefs = startPreferenceFragment();
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
    @EnableFeatures(AssistantFeatures.AUTOFILL_ASSISTANT_NAME)
    public void testAutofillAssistantPreferenceShownIfOnboardingShown() {
        acceptAssistantOnboarding();
        final AutofillAssistantPreferenceFragment prefs = startPreferenceFragment();
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
    @DisableFeatures(AssistantFeatures.AUTOFILL_ASSISTANT_NAME)
    public void testAutofillAssistantNoPreferenceIfFeatureDisabled() {
        acceptAssistantOnboarding();
        final AutofillAssistantPreferenceFragment prefs = startPreferenceFragment();
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
    @EnableFeatures(AssistantFeatures.AUTOFILL_ASSISTANT_NAME)
    public void testAutofillAssistantSwitchOn() {
        acceptAssistantOnboarding();
        final AutofillAssistantPreferenceFragment prefs = startPreferenceFragment();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeSwitchPreference autofillAssistantSwitch =
                    (ChromeSwitchPreference) prefs.findPreference(
                            AutofillAssistantPreferenceFragment.PREF_AUTOFILL_ASSISTANT);
            assertTrue(autofillAssistantSwitch.isChecked());

            autofillAssistantSwitch.performClick();
            assertFalse(getBooleanPref(Pref.AUTOFILL_ASSISTANT_ENABLED));
            autofillAssistantSwitch.performClick();
            assertTrue(getBooleanPref(Pref.AUTOFILL_ASSISTANT_ENABLED));
        });
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    @EnableFeatures({AssistantFeatures.AUTOFILL_ASSISTANT_NAME,
            AssistantFeatures.AUTOFILL_ASSISTANT_PROACTIVE_HELP_NAME})
    @DisableFeatures(AssistantFeatures.AUTOFILL_ASSISTANT_DISABLE_PROACTIVE_HELP_TIED_TO_MSBB_NAME)
    public void
    testProactiveHelpDisabledIfMsbbDisabled() {
        acceptAssistantOnboarding();
        final AutofillAssistantPreferenceFragment prefs = startPreferenceFragment();

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
    @EnableFeatures({AssistantFeatures.AUTOFILL_ASSISTANT_NAME,
            AssistantFeatures.AUTOFILL_ASSISTANT_PROACTIVE_HELP_NAME,
            AssistantFeatures.AUTOFILL_ASSISTANT_DISABLE_PROACTIVE_HELP_TIED_TO_MSBB_NAME})
    public void
    testProactiveHelpNotLinkedToMsbbIfLinkDisabled() {
        final AutofillAssistantPreferenceFragment prefs = startPreferenceFragment();

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
    @EnableFeatures({AssistantFeatures.AUTOFILL_ASSISTANT_NAME,
            AssistantFeatures.AUTOFILL_ASSISTANT_PROACTIVE_HELP_NAME})
    @DisableFeatures(AssistantFeatures.AUTOFILL_ASSISTANT_DISABLE_PROACTIVE_HELP_TIED_TO_MSBB_NAME)
    public void
    testProactiveHelpDisabledIfAutofillAssistantDisabled() {
        acceptAssistantOnboarding();
        final AutofillAssistantPreferenceFragment prefs = startPreferenceFragment();

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
    @DisableFeatures(AssistantFeatures.AUTOFILL_ASSISTANT_PROACTIVE_HELP_NAME)
    public void testProactiveHelpInvisibleIfProactiveHelpDisabled() {
        final AutofillAssistantPreferenceFragment prefs = startPreferenceFragment();

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
    @DisableFeatures({AssistantFeatures.AUTOFILL_ASSISTANT_NAME,
            AssistantFeatures.AUTOFILL_ASSISTANT_PROACTIVE_HELP_NAME})
    public void
    testWebAssistanceInvisibleIfAutofillAssistantCompletelyDisabled() {
        final AutofillAssistantPreferenceFragment prefs = startPreferenceFragment();

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
        final AutofillAssistantPreferenceFragment prefs = startPreferenceFragment();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PreferenceCategory assistantVoiceSearchCategory = prefs.findPreference(
                    AutofillAssistantPreferenceFragment.PREF_ASSISTANT_VOICE_SEARCH_CATEGORY);
            assertTrue(assistantVoiceSearchCategory.isVisible());

            ChromeSwitchPreference assistantVoiceSearchEnabledSwitch =
                    (ChromeSwitchPreference) prefs.findPreference(
                            AutofillAssistantPreferenceFragment
                                    .PREF_ASSISTANT_VOICE_SEARCH_ENABLED_SWITCH);
            assertTrue(assistantVoiceSearchEnabledSwitch.isVisible());
            assistantVoiceSearchEnabledSwitch.performClick();
            assertTrue(mSharedPreferencesManager.readBoolean(
                    ChromePreferenceKeys.ASSISTANT_VOICE_SEARCH_ENABLED, /* default= */ false));
        });
    }

    @Test
    @LargeTest
    @Feature({"AssistantVoiceSearch"})
    @EnableFeatures({ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH,
            ChromeFeatureList.ASSISTANT_NON_PERSONALIZED_VOICE_SEARCH})
    public void
    testEnhancedVoiceSearch_DisabledForNonPersonalizedSearch() {
        final AutofillAssistantPreferenceFragment prefs = startPreferenceFragment();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PreferenceCategory assistantVoiceSearchCategory = prefs.findPreference(
                    AutofillAssistantPreferenceFragment.PREF_ASSISTANT_VOICE_SEARCH_CATEGORY);
            assertFalse(assistantVoiceSearchCategory.isVisible());

            ChromeSwitchPreference assistantVoiceSearchEnabledSwitch =
                    (ChromeSwitchPreference) prefs.findPreference(
                            AutofillAssistantPreferenceFragment
                                    .PREF_ASSISTANT_VOICE_SEARCH_ENABLED_SWITCH);
            assertFalse(assistantVoiceSearchEnabledSwitch.isVisible());
        });
    }

    @Test
    @LargeTest
    @Feature({"AssistantVoiceSearch"})
    @DisableFeatures(ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH)
    public void testEnhancedVoiceSearch_Disabled() {
        final AutofillAssistantPreferenceFragment prefs = startPreferenceFragment();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PreferenceCategory assistantVoiceSearchCategory = prefs.findPreference(
                    AutofillAssistantPreferenceFragment.PREF_ASSISTANT_VOICE_SEARCH_CATEGORY);
            assertFalse(assistantVoiceSearchCategory.isVisible());

            ChromeSwitchPreference assistantVoiceSearchEnabledSwitch =
                    (ChromeSwitchPreference) prefs.findPreference(
                            AutofillAssistantPreferenceFragment
                                    .PREF_ASSISTANT_VOICE_SEARCH_ENABLED_SWITCH);
            assertFalse(assistantVoiceSearchEnabledSwitch.isVisible());
        });
    }

    private AutofillAssistantPreferenceFragment startPreferenceFragment() {
        mSettingsActivityTestRule.startSettingsActivity();
        return mSettingsActivityTestRule.getFragment();
    }
}
