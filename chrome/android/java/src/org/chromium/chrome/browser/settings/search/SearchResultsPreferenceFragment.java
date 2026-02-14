// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.search;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.os.Bundle;
import android.text.TextUtils;
import android.view.View;
import android.widget.TextView;

import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.preference.PreferenceScreen;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.MainSettings;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;

import java.util.ArrayList;

/** A simple Fragment to display a list of search results. */
@NullMarked
public class SearchResultsPreferenceFragment extends ChromeBaseSettingsFragment {
    private static final String KEY_PREFERENCE_DATA = "PreferenceData";

    // All search results fragment instance share a title supplier. This keeps
    // |MultiColumnTitleUpdater| from adding titles every time a new fragment instance is created
    // and replaced with the existing one upon user keystrokes entering queries.
    // TODO(crbug.com/444464896): Avoid using the static instance.
    private static @Nullable SettableMonotonicObservableSupplier<String> sTitleSupplier;

    /** Interface for opening the setting selected from the search results. */
    public interface SelectedCallback {
        /**
         * Callback method invoked when a setting entry is selected.
         *
         * @param preferenceFragment Package name of the Fragment containing the chosen setting.
         * @param key A unique key associated with the chosen setting.
         * @param extras The additional args required to launch the pref.
         * @param highlight Whether or not to highlight the item.
         * @param highlightKey The key to highlight if it is different from {@code key}.
         * @param subViewPos Position of the view to highlight among the child views.
         */
        void onSelected(
                @Nullable String preferenceFragment,
                String key,
                Bundle extras,
                boolean highlight,
                @Nullable String highlightKey,
                int subViewPos);
    }

    private @Nullable ArrayList<SettingsIndexData.Entry> mPreferenceData;
    private @Nullable SelectedCallback mSelectedCallback;

    /**
     * Set preference's data.
     *
     * @param results Search results to display.
     */
    public void setPreferenceData(ArrayList<SettingsIndexData.Entry> results) {
        mPreferenceData = results;
    }

    /**
     * Set callback to use when items are selected.
     *
     * @param selectedCallback A callback invoked when one of the result entries is chosen.
     */
    public void setSelectedCallback(SelectedCallback selectedCallback) {
        mSelectedCallback = selectedCallback;
    }

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        if (savedInstanceState != null) {
            mPreferenceData = savedInstanceState.getParcelableArrayList(KEY_PREFERENCE_DATA);
        }
        if (mPreferenceData == null) {
            throw new IllegalStateException(
                    "Preference data should be set. entries: " + mPreferenceData);
        }

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
            boolean useSummaryAsTitle = (info.title == null);
            preference.setTitle(useSummaryAsTitle ? info.summary : info.title);
            preference.setSummary(useSummaryAsTitle ? null : info.summary);
            preference.setOnPreferenceClickListener(
                    pref -> {
                        // For top-level entries, open the fragment itself, not MainSettings,
                        // and no need to scroll/highlight the item.
                        String mainSettingsFragment = MainSettings.class.getName();
                        var isMain = TextUtils.equals(info.parentFragment, mainSettingsFragment);
                        String fragment = isMain ? info.fragment : info.parentFragment;
                        assumeNonNull(mSelectedCallback)
                                .onSelected(
                                        fragment,
                                        info.key,
                                        info.extras,
                                        !isMain,
                                        info.highlightKey,
                                        info.subViewPos);
                        return true;
                    });
            preference.setIconSpaceReserved(false);
            screen.addPreference(preference);
            prevGroup = group;
        }
    }

    @Override
    public MonotonicObservableSupplier<String> getPageTitle() {
        if (sTitleSupplier == null) {
            var title = assumeNonNull(getContext()).getString(R.string.search_in_settings_results);
            sTitleSupplier = ObservableSuppliers.createMonotonic();
            sTitleSupplier.set(title);
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

    @Override
    public void onViewCreated(View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        getListView().addOnChildAttachStateChangeListener(mChildAttachListener);
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();

        if (getListView() != null) {
            getListView().removeOnChildAttachStateChangeListener(mChildAttachListener);
        }
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        if (mPreferenceData != null) {
            outState.putParcelableArrayList(KEY_PREFERENCE_DATA, mPreferenceData);
        }
    }

    private final RecyclerView.OnChildAttachStateChangeListener mChildAttachListener =
            new RecyclerView.OnChildAttachStateChangeListener() {
                @Override
                public void onChildViewAttachedToWindow(View view) {
                    // Limit Title to 2 lines and append "..."
                    TextView titleView = view.findViewById(android.R.id.title);
                    if (titleView != null) {
                        titleView.setMaxLines(2);
                        titleView.setEllipsize(TextUtils.TruncateAt.END);
                    }

                    // Limit Body (Summary) to 2 lines and append "..."
                    TextView summaryView = view.findViewById(android.R.id.summary);
                    if (summaryView != null) {
                        summaryView.setMaxLines(2);
                        summaryView.setEllipsize(TextUtils.TruncateAt.END);
                    }
                }

                @Override
                public void onChildViewDetachedFromWindow(View view) {}
            };
}
