// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import android.os.Bundle;

import androidx.preference.Preference;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.NullUtil;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.chrome.browser.settings.search.BaseSearchIndexProvider;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsFragment;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.TextMessagePreference;

/**
 * Fragment to manage the Contextual Search Settings in Chrome Settings, and to explain to the user
 * what Contextual Search (aka Touch to Search) actually does.
 */
@NullMarked
public class ContextualSearchSettingsFragment extends ChromeBaseSettingsFragment {
    static final String PREF_CONTEXTUAL_SEARCH_SWITCH = "contextual_search_switch";
    static final String PREF_CONTEXTUAL_SEARCH_DESCRIPTION = "contextual_search_description";
    static final String PREF_WAS_FULLY_ENABLED_SWITCH = "see_better_results_switch";

    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.contextual_search_preferences);
        mPageTitle.set(getString(R.string.contextual_search_title));
        setHasOptionsMenu(true);
        initSwitches();

        if (ChromeFeatureList.sAndroidSettingsContainment.isEnabled()) {
            // TODO(crbug.com/439911511): Set the summary instead of the title in the layout file.
            TextMessagePreference contextualSearchDescription =
                    findPreference(PREF_CONTEXTUAL_SEARCH_DESCRIPTION);
            NullUtil.assertNonNull(contextualSearchDescription)
                    .setSummary(contextualSearchDescription.getTitle());
            contextualSearchDescription.setTitle(null);
        }
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    private void initSwitches() {
        ChromeSwitchPreference contextualSearchSwitch =
                (ChromeSwitchPreference) findPreference(PREF_CONTEXTUAL_SEARCH_SWITCH);
        ChromeSwitchPreference seeBetterResultsSwitch =
                (ChromeSwitchPreference) findPreference(PREF_WAS_FULLY_ENABLED_SWITCH);

        Profile profile = getProfile();
        boolean isContextualSearchEnabled =
                !ContextualSearchPolicy.isContextualSearchDisabled(profile);
        contextualSearchSwitch.setChecked(isContextualSearchEnabled);

        contextualSearchSwitch.setOnPreferenceChangeListener(
                (preference, newValue) -> {
                    ContextualSearchPolicy.setContextualSearchState(profile, (boolean) newValue);
                    ContextualSearchUma.logMainPreferenceChange((boolean) newValue);
                    seeBetterResultsSwitch.setVisible((boolean) newValue);
                    return true;
                });

        contextualSearchSwitch.setManagedPreferenceDelegate(
                new ChromeManagedPreferenceDelegate(profile) {
                    @Override
                    public boolean isPreferenceControlledByPolicy(Preference preference) {
                        return ContextualSearchPolicy.isContextualSearchDisabledByPolicy(profile);
                    }
                });

        seeBetterResultsSwitch.setChecked(
                ContextualSearchPolicy.isContextualSearchPrefFullyOptedIn(profile));
        seeBetterResultsSwitch.setOnPreferenceChangeListener(
                (preference, newValue) -> {
                    ContextualSearchUma.logPrivacyOptInPreferenceChange((boolean) newValue);
                    ContextualSearchPolicy.setContextualSearchFullyOptedIn(
                            profile, (boolean) newValue);
                    return true;
                });

        seeBetterResultsSwitch.setVisible(isContextualSearchEnabled);
    }

    @Override
    public @SettingsFragment.AnimationType int getAnimationType() {
        return SettingsFragment.AnimationType.PROPERTY;
    }

    public static final BaseSearchIndexProvider SEARCH_INDEX_DATA_PROVIDER =
            new BaseSearchIndexProvider(
                    ContextualSearchSettingsFragment.class.getName(),
                    R.xml.contextual_search_preferences);
}
