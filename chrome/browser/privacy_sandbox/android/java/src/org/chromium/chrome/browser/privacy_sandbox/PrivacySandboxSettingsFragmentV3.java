// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.os.Bundle;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.style.ForegroundColorSpan;

import androidx.preference.Preference;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;

/**
 * Settings fragment for privacy sandbox settings.
 */
public class PrivacySandboxSettingsFragmentV3 extends PrivacySandboxSettingsBaseFragment
        implements Preference.OnPreferenceChangeListener {
    public static final String PRIVACY_SANDBOX_URL = "https://www.privacysandbox.com";

    public static final String TOGGLE_PREFERENCE = "privacy_sandbox_toggle";
    public static final String LEARN_MORE_PREFERENCE = "privacy_sandbox_learn_more";

    /**
     * Initializes all the objects related to the preferences page.
     */
    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        assert (!ChromeFeatureList.isEnabled(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4));

        super.onCreatePreferences(bundle, s);

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
                getResources().getString(R.string.privacy_sandbox_about_ad_personalization_link));
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
}
