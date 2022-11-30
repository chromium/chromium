// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import androidx.preference.PreferenceCategory;
import androidx.test.filters.LargeTest;

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
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
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
