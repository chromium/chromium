// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.search;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.os.Bundle;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.preference.PreferenceScreen;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.MainSettings;

import java.util.List;

/** A simple Fragment to display a list of search results. */
@NullMarked
public class SearchResultsPreferenceFragment extends ChromeBaseSettingsFragment {
    // All search results fragment instance share a title supplier. This keeps
    // |MultiColumnTitleUpdater| from adding titles every time a new fragment instance is created
    // and replaced with the existing one upon user keystrokes entering queries.
    // TODO(crbug.com/444464896): Avoid using the static instance.
    private static @Nullable ObservableSupplier<String> sTitleSupplier;

    /** Interface for opening the setting selected from the search results. */
    public interface SelectedCallback {
        /**
         * Callback method invoked when a setting entry is selected.
         *
         * @param preferenceFragment Package name of the Fragment containing the chosen setting.
         * @param key A unique key associated with the chosen setting.
         * @param extras The additional args required to launch the pref.
         */
        void onSelected(String preferenceFragment, String key, Bundle extras);
    }

    private final List<SettingsIndexData.Entry> mPreferenceData;
    private final SelectedCallback mSelectedCallback;

    /**
     * Constructor
     *
     * @param results Search results to display.
     * @param selectedCallback A callback invoked when one of the result entries is chosen.
     */
    public SearchResultsPreferenceFragment(
            List<SettingsIndexData.Entry> results, SelectedCallback selectedCallback) {
        super();
        mPreferenceData = results;
        mSelectedCallback = selectedCallback;
    }

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        PreferenceScreen screen = getPreferenceManager().createPreferenceScreen(requireContext());
        setPreferenceScreen(screen);

        String prevGroup = null;
        for (SettingsIndexData.Entry info : mPreferenceData) {
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
                            mSelectedCallback.onSelected(fragmentToOpen, info.key, info.extras);
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
        if (sTitleSupplier == null) {
            var title = assumeNonNull(getContext()).getString(R.string.search_in_settings_results);
            sTitleSupplier = new ObservableSupplierImpl<String>(title);
        }
        return sTitleSupplier;
    }

    static void reset() {
        sTitleSupplier = null;
    }

    @Override
    public @AnimationType int getAnimationType() {
        return AnimationType.PROPERTY;
    }
}
