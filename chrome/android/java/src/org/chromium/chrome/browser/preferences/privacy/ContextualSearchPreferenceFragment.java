// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.privacy;

import android.os.Bundle;
import android.support.v7.preference.PreferenceFragmentCompat;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManager;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchUma;
import org.chromium.chrome.browser.preferences.ChromeSwitchPreference;
import org.chromium.chrome.browser.preferences.PreferenceUtils;

/**
 * Fragment to manage the Contextual Search preference and to explain to the user what it does.
 */
public class ContextualSearchPreferenceFragment extends PreferenceFragmentCompat {
    private static final String PREF_CONTEXTUAL_SEARCH_SWITCH = "contextual_search_switch";

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        PreferenceUtils.addPreferencesFromResource(this, R.xml.contextual_search_preferences);
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
                preference -> ContextualSearchManager.isContextualSearchDisabledByPolicy());
    }
}
