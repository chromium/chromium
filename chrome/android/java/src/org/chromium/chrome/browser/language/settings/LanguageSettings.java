// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.language.settings;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceGroup;

import org.chromium.base.BuildInfo;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.language.AppLocaleUtils;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.translate.TranslateBridge;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

/**
 * Settings fragment that displays information about Chrome languages, which allow users to
 * seamlessly find and manage their languages preferences across platforms.
 */
public class LanguageSettings
        extends PreferenceFragmentCompat implements AddLanguageFragment.Launcher {
    private static final int REQUEST_CODE_ADD_ACCEPT_LANGUAGE = 1;
    private static final int REQUEST_CODE_CHANGE_APP_LANGUAGE = 2;
    private static final int REQUEST_CODE_CHANGE_TARGET_LANGUAGE = 3;

    // The keys for each preference shown on the languages page.
    static final String APP_LANGUAGE_SECTION_KEY = "app_language_section";
    static final String APP_LANGUAGE_PREFERENCE_KEY = "app_language_preference";
    static final String PREFERRED_LANGUAGES_KEY = "preferred_languages";
    static final String CONTENT_LANGUAGES_KEY = "content_languages_preference";
    static final String TRANSLATE_SWITCH_KEY = "translate_switch";

    static final String TRANSLATION_SETTINGS_SECTION = "translation_settings_section";
    static final String TARGET_LANGUAGE_KEY = "translate_settings_target_language";
    static final String AUTOMATIC_LANGUAGES_KEY = "translate_settings_automatic_languages";
    static final String NO_PROMPT_LANGUAGES_KEY = "translate_settings_no_prompt_languages";
    static final String NO_PROMPT_SITES_KEY = "translate_settings_no_prompt_sites";

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

    /**
     * Create the old language and translate settings page.  Delete once no longer used.
     */
    private void createBasicPreferences(Bundle savedInstanceState, String rootKey) {
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

    /**
     * Create the new language and translate settings page. With options to change the app language,
     * translate target language, and detailed translate preferences.
     */
    private void createDetailedPreferences(Bundle savedInstanceState, String rootKey) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.languages_detailed_preferences);

        setupAppLanguageSection();

        LanguageListPreference mLanguageListPref =
                (LanguageListPreference) findPreference(CONTENT_LANGUAGES_KEY);
        mLanguageListPref.registerActivityLauncher(this);

        setupTranslateSection(mLanguageListPref);
    }

    /**
     * Setup the App Language section with a title and preference to choose the app language.
     */
    private void setupAppLanguageSection() {
        // Set title to include current app name.
        PreferenceCategory mAppLanguageTitle =
                (PreferenceCategory) findPreference(APP_LANGUAGE_SECTION_KEY);
        String mAppName = BuildInfo.getInstance().hostPackageLabel;
        mAppLanguageTitle.setTitle(getResources().getString(R.string.app_language_title, mAppName));

        LanguageItemPickerPreference appLanguagePreference =
                (LanguageItemPickerPreference) findPreference(APP_LANGUAGE_PREFERENCE_KEY);
        appLanguagePreference.setLanguageItem(AppLocaleUtils.getAppLanguagePref());
        appLanguagePreference.useLanguageItemForTitle(true);
        setSelectLanguageLauncher(appLanguagePreference,
                AddLanguageFragment.LANGUAGE_OPTIONS_UI_LANGUAGES, REQUEST_CODE_CHANGE_APP_LANGUAGE,
                LanguagesManager.LanguageSettingsPageType.CHROME_LANGUAGE);
    }

    /**
     * Setup the translate preferences section.  A switch preferences controls if translate is
     * enabled/disabled and will hide all advanced settings when disabled.
     * @param languageListPreference LanguageListPreference reference to update about state changes.
     */
    private void setupTranslateSection(LanguageListPreference languageListPreference) {
        ChromeSwitchPreference translateSwitch =
                (ChromeSwitchPreference) findPreference(TRANSLATE_SWITCH_KEY);
        boolean isTranslateEnabled = getPrefService().getBoolean(Pref.OFFER_TRANSLATE_ENABLED);
        translateSwitch.setChecked(isTranslateEnabled);

        PreferenceCategory translationSettingsSection =
                (PreferenceCategory) findPreference(TRANSLATION_SETTINGS_SECTION);
        translationSettingsSection.setOnExpandButtonClickListener(
                new PreferenceGroup.OnExpandButtonClickListener() {
                    @Override
                    public void onExpandButtonClick() {
                        LanguagesManager.recordImpression(LanguagesManager.LanguageSettingsPageType
                                                                  .ADVANCED_LANGUAGE_SETTINGS);
                    }
                });

        // Get advanced section preference items
        LanguageItemPickerPreference targetLanguagePreference =
                (LanguageItemPickerPreference) findPreference(TARGET_LANGUAGE_KEY);

        translateSwitch.setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener() {
            @Override
            public boolean onPreferenceChange(Preference preference, Object newValue) {
                boolean enabled = (boolean) newValue;
                getPrefService().setBoolean(Pref.OFFER_TRANSLATE_ENABLED, enabled);
                languageListPreference.notifyPrefChanged();
                targetLanguagePreference.setVisible(enabled);
                LanguagesManager.recordAction(enabled ? LanguagesManager.LanguageSettingsActionType
                                                                .ENABLE_TRANSLATE_GLOBALLY
                                                      : LanguagesManager.LanguageSettingsActionType
                                                                .DISABLE_TRANSLATE_GLOBALLY);
                return true;
            }
        });
        translateSwitch.setManagedPreferenceDelegate((ChromeManagedPreferenceDelegate) preference
                -> getPrefService().isManagedPreference(Pref.OFFER_TRANSLATE_ENABLED));

        targetLanguagePreference.setLanguageItem(TranslateBridge.getTargetLanguage());
        setSelectLanguageLauncher(targetLanguagePreference,
                AddLanguageFragment.LANGUAGE_OPTIONS_TRANSLATE_LANGUAGES,
                REQUEST_CODE_CHANGE_TARGET_LANGUAGE,
                LanguagesManager.LanguageSettingsPageType.TARGET_LANGUAGE);
    }

    @Override
    public void onDetach() {
        super.onDetach();
        LanguagesManager.recycle();
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == REQUEST_CODE_ADD_ACCEPT_LANGUAGE && resultCode == Activity.RESULT_OK) {
            String code = data.getStringExtra(AddLanguageFragment.INTENT_SELECTED_LANGUAGE);
            LanguagesManager.getInstance().addToAcceptLanguages(code);
            LanguagesManager.recordAction(
                    LanguagesManager.LanguageSettingsActionType.LANGUAGE_ADDED);
        } else if (requestCode == REQUEST_CODE_CHANGE_APP_LANGUAGE
                && resultCode == Activity.RESULT_OK) {
            String code = data.getStringExtra(AddLanguageFragment.INTENT_SELECTED_LANGUAGE);
            LanguageItemPickerPreference appLanguagePreference =
                    (LanguageItemPickerPreference) findPreference(APP_LANGUAGE_PREFERENCE_KEY);
            appLanguagePreference.setLanguageItem(code);
            AppLocaleUtils.setAppLanguagePref(code);
            LanguagesManager.recordAction(
                    LanguagesManager.LanguageSettingsActionType.CHANGE_CHROME_LANGUAGE);
        } else if (requestCode == REQUEST_CODE_CHANGE_TARGET_LANGUAGE
                && resultCode == Activity.RESULT_OK) {
            String code = data.getStringExtra(AddLanguageFragment.INTENT_SELECTED_LANGUAGE);
            LanguageItemPickerPreference targetLanguagePreference =
                    (LanguageItemPickerPreference) findPreference(TARGET_LANGUAGE_KEY);
            targetLanguagePreference.setLanguageItem(code);
            TranslateBridge.setDefaultTargetLanguage(code);
            LanguagesManager.recordAction(
                    LanguagesManager.LanguageSettingsActionType.CHANGE_TARGET_LANGUAGE);
        }
    }

    /**
     * Overrides AddLanguageFragment.Launcher.launchAddLanguage to handle click events on the
     * Add Language button inside the LanguageListPreference.
     */
    @Override
    public void launchAddLanguage() {
        LanguagesManager.recordImpression(
                LanguagesManager.LanguageSettingsPageType.PAGE_ADD_LANGUAGE);
        launchSelectLanguage(AddLanguageFragment.LANGUAGE_OPTIONS_ACCEPT_LANGUAGES,
                REQUEST_CODE_ADD_ACCEPT_LANGUAGE);
    }

    /**
     * Set preference's OnPreferenceClickListener to launch the Select Language Fragment.
     * @param Preference preference The Preference to set listener on.
     * @param int launchCode The language options code to filter selectable languages.
     * @param int requestCode The code to return from the select language fragment with.
     * @param int pageType The LanguageSettingsPageType to record impression for.
     */
    private void setSelectLanguageLauncher(Preference preference, int launchCode, int requestCode,
            @LanguagesManager.LanguageSettingsPageType int pageType) {
        preference.setOnPreferenceClickListener(new Preference.OnPreferenceClickListener() {
            @Override
            public boolean onPreferenceClick(Preference preference) {
                LanguagesManager.recordImpression(pageType);
                launchSelectLanguage(launchCode, requestCode);
                return true;
            }
        });
    }

    /**
     * Launch the AddLanguageFragment with launch and request codes to select a single language.
     * @param int launchCode The language options code to filter selectable languages.
     * @param int requestCode The code to return from the select language fragment with.
     */
    private void launchSelectLanguage(int launchCode, int requestCode) {
        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        Intent intent = settingsLauncher.createSettingsActivityIntent(
                getActivity(), AddLanguageFragment.class.getName());
        intent.putExtra(AddLanguageFragment.INTENT_LANGUAGE_OPTIONS, launchCode);
        startActivityForResult(intent, requestCode);
    }

    @VisibleForTesting
    static PrefService getPrefService() {
        return UserPrefs.get(Profile.getLastUsedRegularProfile());
    }
}
