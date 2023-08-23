// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.appcompat.widget.SwitchCompat;
import androidx.fragment.app.Fragment;

import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.user_prefs.UserPrefs;

/**
 * Controls the behaviour of the search suggestions privacy guide page.
 */
public class SearchSuggestionsFragment extends Fragment {
    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return inflater.inflate(R.layout.privacy_guide_search_suggestions_step, container, false);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        SwitchCompat searchSuggestionsSwitch = view.findViewById(R.id.search_suggestions_switch);
        searchSuggestionsSwitch.setChecked(PrivacyGuideUtils.isSearchSuggestionsEnabled());

        searchSuggestionsSwitch.setOnCheckedChangeListener((button, isChecked) -> {
            PrivacyGuideMetricsDelegate.recordMetricsOnSearchSuggestionsChange(isChecked);
            UserPrefs.get(Profile.getLastUsedRegularProfile())
                    .setBoolean(Pref.SEARCH_SUGGEST_ENABLED, isChecked);
        });
    }
}
