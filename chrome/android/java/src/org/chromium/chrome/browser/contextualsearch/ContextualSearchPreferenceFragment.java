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
    private static final String PREF_CONTEXTUAL_SEARCH_SWITCH = "contextual_search_switch";

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
        initContextualSearchSwitch();
    }

    private void initContextualSearchSwitch() {
        ChromeSwitchPreference contextualSearchSwitch =
                (ChromeSwitchPreference) findPreference(PREF_CONTEXTUAL_SEARCH_SWITCH);

        boolean isContextualSearchEnabled = !ContextualSearchManager.isContextualSearchDisabled();
        contextualSearchSwitch.setChecked(isContextualSearchEnabled);

        contextualSearchSwitch.setOnPreferenceChangeListener((preference, newValue) -> {
            ContextualSearchManager.setContextualSearchState((boolean) newValue);
            ContextualSearchUma.logPreferenceChange((boolean) newValue);
            return true;
        });
        contextualSearchSwitch.setManagedPreferenceDelegate(
                (ChromeManagedPreferenceDelegate)
                        preference -> ContextualSearchManager.isContextualSearchDisabledByPolicy());
    }
}
