// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.content.Context;
import android.os.Bundle;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.style.ForegroundColorSpan;

import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.settings.SettingsUtils;

/**
 * Settings fragment for privacy sandbox settings.
 */
public class PrivacySandboxSettingsFragmentV3
        extends PreferenceFragmentCompat implements Preference.OnPreferenceChangeListener {
    public static final String PRIVACY_SANDBOX_URL = "https://www.privacysandbox.com";
    // Key for the argument with which the PrivacySandbox fragment will be launched. The value for
    // this argument should be part of the PrivacySandboxReferrer enum, which contains all points of
    // entry to the Privacy Sandbox UI.
    public static final String PRIVACY_SANDBOX_REFERRER = "privacy-sandbox-referrer";

    public static final String TOGGLE_PREFERENCE = "privacy_sandbox_toggle";
    public static final String LEARN_MORE_PREFERENCE = "privacy_sandbox_learn_more";

    /**
     * Launches the right version of PrivacySandboxSettings depending on feature flags.
     */
    public static void launchPrivacySandboxSettings(Context context,
            SettingsLauncher settingsLauncher, @PrivacySandboxReferrer int referrer) {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putInt(PRIVACY_SANDBOX_REFERRER, referrer);
        Class<? extends PreferenceFragmentCompat> fragment =
                ChromeFeatureList.isEnabled(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_3)
                ? PrivacySandboxSettingsFragmentV3.class
                : PrivacySandboxSettingsFragment.class;
        settingsLauncher.launchSettingsActivity(context, fragment, fragmentArgs);
    }

    /**
     * Initializes all the objects related to the preferences page.
     */
    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        assert ChromeFeatureList.isEnabled(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_3);

        // Add all preferences and set the title.
        getActivity().setTitle(R.string.prefs_privacy_sandbox);
        SettingsUtils.addPreferencesFromResource(this, R.xml.privacy_sandbox_preferences_v3);

        ChromeSwitchPreference privacySandboxToggle =
                (ChromeSwitchPreference) findPreference(TOGGLE_PREFERENCE);
        assert privacySandboxToggle != null;
        privacySandboxToggle.setOnPreferenceChangeListener(this);
        privacySandboxToggle.setManagedPreferenceDelegate(createManagedPreferenceDelegate());
        privacySandboxToggle.setChecked(PrivacySandboxBridge.isPrivacySandboxEnabled());

        ChromeBasePreference learnMorePreference = findPreference(LEARN_MORE_PREFERENCE);
        SpannableString spannableString = new SpannableString(
                getResources().getString(R.string.privacy_sandbox_consent_dropdown_button));
        spannableString.setSpan(new ForegroundColorSpan(getContext().getColor(
                                        R.color.default_text_color_link_baseline)),
                0, spannableString.length(), Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
        learnMorePreference.setSummary(spannableString);

        parseAndRecordReferrer();
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        if (TOGGLE_PREFERENCE.equals(preference.getKey())) {
            boolean enabled = (boolean) newValue;
            RecordUserAction.record(enabled ? "Settings.PrivacySandbox.ApisEnabled"
                                            : "Settings.PrivacySandbox.ApisDisabled");
            PrivacySandboxBridge.setPrivacySandboxEnabled(enabled);
        }
        return true;
    }

    private ChromeManagedPreferenceDelegate createManagedPreferenceDelegate() {
        return preference -> {
            if (TOGGLE_PREFERENCE.equals(preference.getKey())) {
                return PrivacySandboxBridge.isPrivacySandboxManaged();
            }
            return false;
        };
    }

    private void parseAndRecordReferrer() {
        Bundle extras = getArguments();
        assert (extras != null)
                && extras.containsKey(PRIVACY_SANDBOX_REFERRER)
            : "PrivacySandboxSettingsFragment must be launched with a privacy-sandbox-referrer "
                        + "fragment argument, but none was provided.";
        int referrer = extras.getInt(PRIVACY_SANDBOX_REFERRER);
        // Record all the referrer metrics.
        RecordHistogram.recordEnumeratedHistogram("Settings.PrivacySandbox.PrivacySandboxReferrer",
                referrer, PrivacySandboxReferrer.COUNT);
        if (referrer == PrivacySandboxReferrer.PRIVACY_SETTINGS) {
            RecordUserAction.record("Settings.PrivacySandbox.OpenedFromSettingsParent");
        } else if (referrer == PrivacySandboxReferrer.COOKIES_SNACKBAR) {
            RecordUserAction.record("Settings.PrivacySandbox.OpenedFromCookiesPageToast");
        }
    }
}
