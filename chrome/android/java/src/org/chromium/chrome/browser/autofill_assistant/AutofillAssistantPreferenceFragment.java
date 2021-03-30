// Copyright 2020 The Chromium Authors. All rights reserved.
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
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.sync.settings.GoogleServicesSettings;
import org.chromium.chrome.browser.sync.settings.SyncAndServicesSettings;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

/**
 * Settings fragment for Autofill Assistant.
 */
public class AutofillAssistantPreferenceFragment extends PreferenceFragmentCompat {
    @VisibleForTesting
    public static final String PREF_WEB_ASSISTANCE_CATEGORY = "web_assistance";
    @VisibleForTesting
    public static final String PREF_AUTOFILL_ASSISTANT = "autofill_assistant_switch";
    @VisibleForTesting
    public static final String PREF_ASSISTANT_PROACTIVE_HELP_SWITCH = "proactive_help_switch";
    @VisibleForTesting
    public static final String PREF_GOOGLE_SERVICES_SETTINGS_LINK = "google_services_settings_link";
    @VisibleForTesting
    public static final String PREF_ASSISTANT_VOICE_SEARCH_CATEGORY = "voice_assistance";
    @VisibleForTesting
    public static final String PREF_ASSISTANT_VOICE_SEARCH_ENABLED_SWTICH =
            "voice_assistance_enabled";

    private final SharedPreferencesManager mSharedPreferencesManager =
            SharedPreferencesManager.getInstance();

    private PreferenceCategory mWebAssistanceCategory;
    private ChromeSwitchPreference mAutofillAssistantPreference;
    private ChromeSwitchPreference mProactiveHelpPreference;
    private ChromeSwitchPreference mAssistantVoiceSearchEnabledPref;
    private Preference mGoogleServicesSettingsLink;

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.autofill_assistant_preferences);
        getActivity().setTitle(R.string.prefs_autofill_assistant_title);

        mWebAssistanceCategory = (PreferenceCategory) findPreference(PREF_WEB_ASSISTANCE_CATEGORY);
        if (!shouldShowWebAssistanceCategory()) {
            mWebAssistanceCategory.setVisible(false);
        }

        mAutofillAssistantPreference =
                (ChromeSwitchPreference) findPreference(PREF_AUTOFILL_ASSISTANT);
        if (shouldShowAutofillAssistantPreference()) {
            mAutofillAssistantPreference.setOnPreferenceChangeListener((preference, newValue) -> {
                mSharedPreferencesManager.writeBoolean(
                        ChromePreferenceKeys.AUTOFILL_ASSISTANT_ENABLED, (boolean) newValue);
                updatePreferencesState();
                return true;
            });
        } else {
            mAutofillAssistantPreference.setVisible(false);
        }

        mProactiveHelpPreference =
                (ChromeSwitchPreference) findPreference(PREF_ASSISTANT_PROACTIVE_HELP_SWITCH);
        if (shouldShowAutofillAssistantProactiveHelpPreference()) {
            mProactiveHelpPreference.setOnPreferenceChangeListener((preference, newValue) -> {
                mSharedPreferencesManager.writeBoolean(
                        ChromePreferenceKeys.AUTOFILL_ASSISTANT_PROACTIVE_HELP, (boolean) newValue);
                updatePreferencesState();
                return true;
            });
        } else {
            mProactiveHelpPreference.setVisible(false);
        }

        mGoogleServicesSettingsLink = findPreference(PREF_GOOGLE_SERVICES_SETTINGS_LINK);
        NoUnderlineClickableSpan linkSpan = new NoUnderlineClickableSpan(getResources(), view -> {
            SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)) {
                settingsLauncher.launchSettingsActivity(
                        getActivity(), GoogleServicesSettings.class);
            } else {
                settingsLauncher.launchSettingsActivity(getActivity(),
                        SyncAndServicesSettings.class,
                        SyncAndServicesSettings.createArguments(false));
            }
        });
        mGoogleServicesSettingsLink.setSummary(
                SpanApplier.applySpans(getString(R.string.prefs_proactive_help_sync_link),
                        new SpanApplier.SpanInfo("<link>", "</link>", linkSpan)));

        PreferenceCategory assistantVoiceSearchCategory =
                findPreference(PREF_ASSISTANT_VOICE_SEARCH_CATEGORY);
        mAssistantVoiceSearchEnabledPref =
                (ChromeSwitchPreference) findPreference(PREF_ASSISTANT_VOICE_SEARCH_ENABLED_SWTICH);
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH)) {
            mAssistantVoiceSearchEnabledPref.setOnPreferenceChangeListener((preference,
                                                                                   newValue) -> {
                SharedPreferencesManager.getInstance().writeBoolean(
                        ChromePreferenceKeys.ASSISTANT_VOICE_SEARCH_ENABLED, (boolean) newValue);

                return true;
            });
        } else {
            assistantVoiceSearchCategory.setVisible(false);
            mAssistantVoiceSearchEnabledPref.setVisible(false);
        }

        updatePreferencesState();
    }

    @Override
    public void onResume() {
        super.onResume();
        updatePreferencesState();
    }

    private boolean shouldShowAutofillAssistantPreference() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_ASSISTANT)
                && mSharedPreferencesManager.contains(
                        ChromePreferenceKeys.AUTOFILL_ASSISTANT_ENABLED);
    }

    private boolean shouldShowAutofillAssistantProactiveHelpPreference() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_ASSISTANT_PROACTIVE_HELP);
    }

    private boolean shouldShowWebAssistanceCategory() {
        return shouldShowAutofillAssistantProactiveHelpPreference()
                || shouldShowAutofillAssistantPreference();
    }

    private void updatePreferencesState() {
        boolean autofill_assistant_enabled = SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.AUTOFILL_ASSISTANT_ENABLED, true);
        mAutofillAssistantPreference.setChecked(autofill_assistant_enabled);

        boolean assistant_switch_on_or_missing =
                !mAutofillAssistantPreference.isVisible() || autofill_assistant_enabled;
        boolean url_keyed_anonymized_data_collection_enabled =
                UnifiedConsentServiceBridge.isUrlKeyedAnonymizedDataCollectionEnabled(
                        Profile.getLastUsedRegularProfile());

        boolean proactive_help_on = SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.AUTOFILL_ASSISTANT_PROACTIVE_HELP, true);
        boolean proactive_toggle_enabled;
        boolean show_disclaimer;
        if (ChromeFeatureList.isEnabled(
                    ChromeFeatureList.AUTOFILL_ASSISTANT_DISABLE_PROACTIVE_HELP_TIED_TO_MSBB)) {
            proactive_toggle_enabled = assistant_switch_on_or_missing;
            show_disclaimer = false;
        } else {
            proactive_toggle_enabled =
                    url_keyed_anonymized_data_collection_enabled && assistant_switch_on_or_missing;
            show_disclaimer = !proactive_toggle_enabled && assistant_switch_on_or_missing;
        }
        mProactiveHelpPreference.setEnabled(proactive_toggle_enabled);
        mProactiveHelpPreference.setChecked(proactive_toggle_enabled && proactive_help_on);
        mGoogleServicesSettingsLink.setVisible(show_disclaimer);

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
