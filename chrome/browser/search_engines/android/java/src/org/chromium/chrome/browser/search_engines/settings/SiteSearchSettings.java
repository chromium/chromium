// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings;

import android.os.Bundle;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.chrome.browser.search_engines.settings.custom_search_engine.CustomSearchEngineListCoordinator;
import org.chromium.chrome.browser.search_engines.settings.custom_search_engine.CustomSearchEngineListPreference;
import org.chromium.chrome.browser.search_engines.settings.custom_site_search.CustomSiteSearchCoordinator;
import org.chromium.chrome.browser.search_engines.settings.custom_site_search.CustomSiteSearchListPreference;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.components.browser_ui.settings.SettingsFragment;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;

/**
 * Fragment for the "Manage search engines and site search" settings page. Displays lists of
 * standard search engines and custom site search shortcuts.
 */
@NullMarked
public class SiteSearchSettings extends ChromeBaseSettingsFragment {
    // TODO(crbug.com/478726836): See if this needs to be added to the search index
    private static final String CUSTOM_SEARCH_ENGINE_LIST_PREF = "custom_search_engine_item_list";
    private static final String CUSTOM_SITE_SEARCH_LIST_PREF = "custom_site_search_item_list";
    private final SettableMonotonicObservableSupplier<String> mPageTitle =
            ObservableSuppliers.createMonotonic();

    private @Nullable CustomSearchEngineListCoordinator mSearchEngineCoordinator;
    private @Nullable CustomSiteSearchCoordinator mSiteSearchCoordinator;

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        mPageTitle.set(getString(R.string.manage_search_engines_and_site_search));
        SettingsUtils.addPreferencesFromResource(this, R.xml.custom_search_engine_preferences);
        CustomSearchEngineListPreference customSearchEnginePref =
                findPreference(CUSTOM_SEARCH_ENGINE_LIST_PREF);
        if (customSearchEnginePref != null) {
            if (mSearchEngineCoordinator == null) {
                mSearchEngineCoordinator =
                        new CustomSearchEngineListCoordinator(
                                getContext(),
                                getProfile(),
                                customSearchEnginePref,
                                ((ModalDialogManagerHolder) getActivity()).getModalDialogManager());
            }
        }

        SettingsUtils.addPreferencesFromResource(this, R.xml.custom_site_search_preferences);
        CustomSiteSearchListPreference customSiteSearchPref =
                findPreference(CUSTOM_SITE_SEARCH_LIST_PREF);
        if (customSiteSearchPref != null) {
            if (mSiteSearchCoordinator == null) {
                mSiteSearchCoordinator = new CustomSiteSearchCoordinator(getContext());
            }
        }
    }

    @Override
    public MonotonicObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public @SettingsFragment.AnimationType int getAnimationType() {
        return SettingsFragment.AnimationType.PROPERTY;
    }

    @Override
    public void onDestroy() {
        if (mSearchEngineCoordinator != null) {
            mSearchEngineCoordinator.destroy();
            mSearchEngineCoordinator = null;
        }
        if (mSiteSearchCoordinator != null) {
            mSiteSearchCoordinator.destroy();
            mSiteSearchCoordinator = null;
        }
        super.onDestroy();
    }
}
