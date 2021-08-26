// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import android.os.Bundle;

import androidx.annotation.XmlRes;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;

/**
 * Fragment to manage the Contextual Search preference in Chrome Settings, and to explain to the
 * user what Contextual Search (aka Touch to Search) actually does.
 */
public class ContextualSearchPreferenceFragment extends PreferenceFragmentCompat {
    static final String PREF_CONTEXTUAL_SEARCH_SWITCH = "contextual_search_switch";
    static final String PREF_WAS_FULLY_ENABLED_SWITCH = "see_better_results_switch";

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        @XmlRes
        int tapOrTouchPreferenceId =
                ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXTUAL_SEARCH_LONGPRESS_RESOLVE)
                ? R.xml.contextual_search_preferences
                : R.xml.contextual_search_tap_preferences;
        SettingsUtils.addPreferencesFromResource(this, tapOrTouchPreferenceId);
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
            ContextualSearchUma.logPreferenceChange((boolean) newValue);
            seeBetterResultsSwitch.setVisible(
                    ContextualSearchPolicy.shouldShowMultilevelSettingsUI() && (boolean) newValue);
            return true;
        });

        contextualSearchSwitch.setManagedPreferenceDelegate(
                (ChromeManagedPreferenceDelegate)
                        preference -> ContextualSearchPolicy.isContextualSearchDisabledByPolicy());

        seeBetterResultsSwitch.setChecked(ContextualSearchPolicy.isContextualSearchFullyOptedIn());
        seeBetterResultsSwitch.setOnPreferenceChangeListener((preference, newValue) -> {
            ContextualSearchPolicy.setContextualSearchFullyOptedIn((boolean) newValue);
            return true;
        });

        seeBetterResultsSwitch.setVisible(ContextualSearchPolicy.shouldShowMultilevelSettingsUI()
                && isContextualSearchEnabled);
    }
}
