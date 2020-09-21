// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.language.settings;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.chrome.browser.settings.SettingsLauncher;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

/**
 * Settings fragment that displays information about Chrome languages, which allow users to
 * seamlessly find and manage their languages preferences across platforms.
 */
public class LanguageSettings
        extends PreferenceFragmentCompat implements AddLanguageFragment.Launcher {
    private static final int REQUEST_CODE_ADD_LANGUAGES = 1;

    // The keys for each preference shown on the languages page.
    static final String PREFERRED_LANGUAGES_KEY = "preferred_languages";
    static final String TRANSLATE_SWITCH_KEY = "translate_switch";

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        getActivity().setTitle(R.string.language_settings);

        // Show the detailed language settings if DETAILED_LANGUAGE_SETTINGS feature is enabled
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.DETAILED_LANGUAGE_SETTINGS)) {
            createDetailedPreferences(savedInstanceState, rootKey);
        } else {
            createBasicPreferences(savedInstanceState, rootKey);
        }

        LanguagesManager.recordImpression(LanguagesManager.LanguageSettingsPageType.PAGE_MAIN);
    }

    public void createDetailedPreferences(Bundle savedInstanceState, String rootKey) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.languages_detailed_preferences);

        DetailedLanguageListPreference mLanguageListPref =
                (DetailedLanguageListPreference) findPreference(PREFERRED_LANGUAGES_KEY);

        mLanguageListPref.registerActivityLauncher(this);

        ChromeSwitchPreference translateSwitch =
                (ChromeSwitchPreference) findPreference(TRANSLATE_SWITCH_KEY);
        boolean isTranslateEnabled = getPrefService().getBoolean(Pref.OFFER_TRANSLATE_ENABLED);
        translateSwitch.setChecked(isTranslateEnabled);

        translateSwitch.setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener() {
            @Override
            public boolean onPreferenceChange(Preference preference, Object newValue) {
                boolean enabled = (boolean) newValue;
                getPrefService().setBoolean(Pref.OFFER_TRANSLATE_ENABLED, enabled);
                mLanguageListPref.notifyPrefChanged();
                LanguagesManager.recordAction(enabled ? LanguagesManager.LanguageSettingsActionType
                                                                .ENABLE_TRANSLATE_GLOBALLY
                                                      : LanguagesManager.LanguageSettingsActionType
                                                                .DISABLE_TRANSLATE_GLOBALLY);
                return true;
            }
        });
        translateSwitch.setManagedPreferenceDelegate((ChromeManagedPreferenceDelegate) preference
                -> getPrefService().isManagedPreference(Pref.OFFER_TRANSLATE_ENABLED));
    }

    public void createBasicPreferences(Bundle savedInstanceState, String rootKey) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.languages_preferences);

        LanguageListPreference mLanguageListPref =
                (LanguageListPreference) findPreference(PREFERRED_LANGUAGES_KEY);

        mLanguageListPref.registerActivityLauncher(this);

        ChromeSwitchPreference translateSwitch =
                (ChromeSwitchPreference) findPreference(TRANSLATE_SWITCH_KEY);
        boolean isTranslateEnabled = getPrefService().getBoolean(Pref.OFFER_TRANSLATE_ENABLED);
        translateSwitch.setChecked(isTranslateEnabled);

        translateSwitch.setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener() {
            @Override
            public boolean onPreferenceChange(Preference preference, Object newValue) {
                boolean enabled = (boolean) newValue;
                getPrefService().setBoolean(Pref.OFFER_TRANSLATE_ENABLED, enabled);
                mLanguageListPref.notifyPrefChanged();
                LanguagesManager.recordAction(enabled ? LanguagesManager.LanguageSettingsActionType
                                                                .ENABLE_TRANSLATE_GLOBALLY
                                                      : LanguagesManager.LanguageSettingsActionType
                                                                .DISABLE_TRANSLATE_GLOBALLY);
                return true;
            }
        });
        translateSwitch.setManagedPreferenceDelegate((ChromeManagedPreferenceDelegate) preference
                -> getPrefService().isManagedPreference(Pref.OFFER_TRANSLATE_ENABLED));
    }

    @Override
    public void onDetach() {
        super.onDetach();
        LanguagesManager.recycle();
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == REQUEST_CODE_ADD_LANGUAGES && resultCode == Activity.RESULT_OK) {
            String code = data.getStringExtra(AddLanguageFragment.INTENT_NEW_ACCEPT_LANGUAGE);
            LanguagesManager.getInstance().addToAcceptLanguages(code);
            LanguagesManager.recordAction(
                    LanguagesManager.LanguageSettingsActionType.LANGUAGE_ADDED);
        }
    }

    @Override
    public void launchAddLanguage() {
        // Launch preference activity with AddLanguageFragment.
        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        Intent intent = settingsLauncher.createSettingsActivityIntent(
                getActivity(), AddLanguageFragment.class.getName());
        startActivityForResult(intent, REQUEST_CODE_ADD_LANGUAGES);
    }

    @VisibleForTesting
    static PrefService getPrefService() {
        return UserPrefs.get(Profile.getLastUsedRegularProfile());
    }
}
