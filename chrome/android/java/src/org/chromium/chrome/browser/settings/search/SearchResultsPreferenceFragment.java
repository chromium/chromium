// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.search;

import android.os.Bundle;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.preference.PreferenceScreen;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.MainSettings;
import org.chromium.chrome.browser.settings.search.SettingsIndexData.SearchResults;

/** A simple Fragment to display a list of search results. */
@NullMarked
public class SearchResultsPreferenceFragment extends ChromeBaseSettingsFragment {

    /** Interface for opening the setting selected from the search results. */
    public interface SelectedCallback {
        /**
         * Callback method invoked when a setting entry is selected.
         *
         * @param preferenceFragment Package name of the Fragment containing the chosen setting.
         * @param key A unique key associated with the chosen setting.
         */
        void onSelected(String preferenceFragment, String key);
    }

    private final SearchResults mPreferenceData;
    private final SelectedCallback mSelectedCallback;
    private @Nullable ObservableSupplier<String> mTitleSupplier;

    /**
     * Constructor
     *
     * @param results Search results to display.
     * @param selectedCallback A callback invoked when one of the result entries is chosen.
     */
    public SearchResultsPreferenceFragment(
            SearchResults results, SelectedCallback selectedCallback) {
        super();
        mPreferenceData = results;
        mSelectedCallback = selectedCallback;
    }

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        PreferenceScreen screen = getPreferenceManager().createPreferenceScreen(requireContext());
        setPreferenceScreen(screen);

        String prevGroup = null;
        for (SettingsIndexData.Entry info : mPreferenceData.getItems()) {
            String group = info.header;

            // The results are grouped by the top level setting categories. Build the category
            // header above the group.
            if (!TextUtils.equals(group, prevGroup)) {
                PreferenceCategory prefGroup = new PreferenceCategory(requireContext());
                prefGroup.setTitle(group);
                prefGroup.setIconSpaceReserved(false);
                screen.addPreference(prefGroup);
            }
            Preference preference = new Preference(requireContext());
            preference.setKey(info.key);
            preference.setTitle(info.title);
            preference.setSummary(info.summary);
            preference.setOnPreferenceClickListener(
                    pref -> {
                        // For top-level entries, open the fragment itself, not MainSettings.
                        String fragmentToOpen = info.parentFragment;
                        if (TextUtils.equals(info.parentFragment, MainSettings.class.getName())) {
                            fragmentToOpen = info.fragment;
                        }
                        if (fragmentToOpen != null) {
                            mSelectedCallback.onSelected(fragmentToOpen, info.key);
                        }
                        return true;
                    });
            preference.setIconSpaceReserved(false);
            screen.addPreference(preference);
            prevGroup = group;
        }
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        if (mTitleSupplier == null) mTitleSupplier = new ObservableSupplierImpl<>();
        return mTitleSupplier;
    }

    @Override
    public @AnimationType int getAnimationType() {
        return AnimationType.PROPERTY;
    }
}
