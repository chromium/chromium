// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.languages;

import android.content.Intent;
import android.os.Bundle;
import android.support.v7.preference.Preference;
import android.support.v7.preference.PreferenceFragmentCompat;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.ChromeSwitchPreference;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.PreferenceUtils;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;

/**
 * Settings fragment that displays information about Chrome languages, which allow users to
 * seamlessly find and manage their languages preferences across platforms.
 */
public class LanguagesPreferences
        extends PreferenceFragmentCompat implements AddLanguageFragment.Launcher {
    private static final int REQUEST_CODE_ADD_LANGUAGES = 1;

    // The keys for each preference shown on the languages page.
    static final String PREFERRED_LANGUAGES_KEY = "preferred_languages";
    static final String TRANSLATE_SWITCH_KEY = "translate_switch";

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        getActivity().setTitle(R.string.prefs_languages);
        PreferenceUtils.addPreferencesFromResource(this, R.xml.languages_preferences);

        LanguageListPreference mLanguageListPref =
                (LanguageListPreference) findPreference(PREFERRED_LANGUAGES_KEY);
        mLanguageListPref.registerActivityLauncher(this);

        ChromeSwitchPreference translateSwitch =
                (ChromeSwitchPreference) findPreference(TRANSLATE_SWITCH_KEY);
        boolean isTranslateEnabled =
                PrefServiceBridge.getInstance().getBoolean(Pref.OFFER_TRANSLATE_ENABLED);
        translateSwitch.setChecked(isTranslateEnabled);

        translateSwitch.setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener() {
            @Override
            public boolean onPreferenceChange(Preference preference, Object newValue) {
                boolean enabled = (boolean) newValue;
                PrefServiceBridge.getInstance().setBoolean(Pref.OFFER_TRANSLATE_ENABLED, enabled);
                LanguagesManager.recordAction(enabled ? LanguagesManager.LanguageSettingsActionType
                                                                .ENABLE_TRANSLATE_GLOBALLY
                                                      : LanguagesManager.LanguageSettingsActionType
                                                                .DISABLE_TRANSLATE_GLOBALLY);
                return true;
            }
        });
        translateSwitch.setManagedPreferenceDelegate(preference
                -> PrefServiceBridge.getInstance().isManagedPreference(
                        Pref.OFFER_TRANSLATE_ENABLED));
        LanguagesManager.recordImpression(LanguagesManager.LanguageSettingsPageType.PAGE_MAIN);
    }

    @Override
    public void onDetach() {
        super.onDetach();
        LanguagesManager.recycle();
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == REQUEST_CODE_ADD_LANGUAGES && resultCode == getActivity().RESULT_OK) {
            String code = data.getStringExtra(AddLanguageFragment.INTENT_NEW_ACCEPT_LANGUAGE);
            LanguagesManager.getInstance().addToAcceptLanguages(code);
            LanguagesManager.recordAction(
                    LanguagesManager.LanguageSettingsActionType.LANGUAGE_ADDED);
        }
    }

    @Override
    public void launchAddLanguage() {
        // Launch preference activity with AddLanguageFragment.
        Intent intent = PreferencesLauncher.createIntentForSettingsPage(
                getActivity(), AddLanguageFragment.class.getName());
        startActivityForResult(intent, REQUEST_CODE_ADD_LANGUAGES);
    }
}
