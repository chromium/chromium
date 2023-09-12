// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import android.os.Bundle;

import androidx.preference.Preference;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;

/**
 * Fragment to manage the Contextual Search preference in Chrome Settings, and to explain to the
 * user what Contextual Search (aka Touch to Search) actually does.
 */
public class ContextualSearchPreferenceFragment extends ChromeBaseSettingsFragment {
    static final String PREF_CONTEXTUAL_SEARCH_SWITCH = "contextual_search_switch";
    static final String PREF_WAS_FULLY_ENABLED_SWITCH = "see_better_results_switch";

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.contextual_search_preferences);
        getActivity().setTitle(R.string.contextual_search_title);
        setHasOptionsMenu(true);
        initSwitches();
    }

    private void initSwitches() {
        ChromeSwitchPreference contextualSearchSwitch =
                (ChromeSwitchPreference) findPreference(PREF_CONTEXTUAL_SEARCH_SWITCH);
        ChromeSwitchPreference seeBetterResultsSwitch =
                (ChromeSwitchPreference) findPreference(PREF_WAS_FULLY_ENABLED_SWITCH);

        boolean isContextualSearchEnabled = !ContextualSearchPolicy.isContextualSearchDisabled();
        contextualSearchSwitch.setChecked(isContextualSearchEnabled);

        contextualSearchSwitch.setOnPreferenceChangeListener((preference, newValue) -> {
            ContextualSearchPolicy.setContextualSearchState((boolean) newValue);
            ContextualSearchUma.logMainPreferenceChange((boolean) newValue);
            seeBetterResultsSwitch.setVisible((boolean) newValue);
            return true;
        });

        contextualSearchSwitch.setManagedPreferenceDelegate(new ChromeManagedPreferenceDelegate(
                getProfile()) {
            @Override
            public boolean isPreferenceControlledByPolicy(Preference preference) {
                return ContextualSearchPolicy.isContextualSearchDisabledByPolicy();
            }
        });

        seeBetterResultsSwitch.setChecked(
                ContextualSearchPolicy.isContextualSearchPrefFullyOptedIn());
        seeBetterResultsSwitch.setOnPreferenceChangeListener((preference, newValue) -> {
            ContextualSearchUma.logPrivacyOptInPreferenceChange((boolean) newValue);
            ContextualSearchPolicy.setContextualSearchFullyOptedIn((boolean) newValue);
            return true;
        });

        seeBetterResultsSwitch.setVisible(isContextualSearchEnabled);
    }
}
