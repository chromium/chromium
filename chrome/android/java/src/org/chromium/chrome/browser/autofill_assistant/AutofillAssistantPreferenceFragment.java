// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.content.Context;
import android.os.Bundle;

import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;

/**
 * Settings fragment for Autofill Assistant.
 */
public class AutofillAssistantPreferenceFragment
        extends PreferenceFragmentCompat implements Preference.OnPreferenceChangeListener {
    @VisibleForTesting
    public static final String PREF_ASSISTANT_VOICE_SEARCH_CATEGORY = "voice_assistance";
    @VisibleForTesting
    public static final String PREF_ASSISTANT_VOICE_SEARCH_ENABLED_SWITCH =
            "voice_assistance_enabled";

    /** SharedPreferences that are used for Assistant voice search settings. */
    private final SharedPreferencesManager mSharedPreferencesManager =
            SharedPreferencesManager.getInstance();

    private ChromeSwitchPreference mAssistantVoiceSearchEnabledPref;

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.autofill_assistant_preferences);
        getActivity().setTitle(R.string.prefs_autofill_assistant_title);

        PreferenceCategory assistantVoiceSearchCategory =
                findPreference(PREF_ASSISTANT_VOICE_SEARCH_CATEGORY);
        mAssistantVoiceSearchEnabledPref =
                (ChromeSwitchPreference) findPreference(PREF_ASSISTANT_VOICE_SEARCH_ENABLED_SWITCH);

        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.ASSISTANT_NON_PERSONALIZED_VOICE_SEARCH)
                && ChromeFeatureList.isEnabled(ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH)) {
            mAssistantVoiceSearchEnabledPref.setOnPreferenceChangeListener(this);
        } else {
            assistantVoiceSearchCategory.setVisible(false);
            mAssistantVoiceSearchEnabledPref.setVisible(false);
        }

        updatePreferencesState();
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        switch (preference.getKey()) {
            case PREF_ASSISTANT_VOICE_SEARCH_ENABLED_SWITCH:
                SharedPreferencesManager.getInstance().writeBoolean(
                        ChromePreferenceKeys.ASSISTANT_VOICE_SEARCH_ENABLED, (boolean) newValue);
                break;
        }
        return true;
    }

    @Override
    public void onResume() {
        super.onResume();
        updatePreferencesState();
    }

    private void updatePreferencesState() {
        mAssistantVoiceSearchEnabledPref.setChecked(mSharedPreferencesManager.readBoolean(
                ChromePreferenceKeys.ASSISTANT_VOICE_SEARCH_ENABLED, /* default= */ false));
    }

    /** Open a page to learn more about the consent dialog. */
    public static void launchSettings(Context context) {
        SettingsLauncherImpl settingsLauncher = new SettingsLauncherImpl();
        settingsLauncher.launchSettingsActivity(
                context, AutofillAssistantPreferenceFragment.class, /* fragmentArgs= */ null);
    }
}
