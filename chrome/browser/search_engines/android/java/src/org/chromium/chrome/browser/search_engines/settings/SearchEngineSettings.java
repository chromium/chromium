// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings;

import android.content.Context;
import android.os.Bundle;
import android.view.View;
import android.widget.ListView;

import androidx.fragment.app.ListFragment;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.regional_capabilities.RegionalCapabilitiesServiceFactory;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.chrome.browser.settings.ProfileDependentSetting;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.settings.search.ChromeBaseSearchIndexProvider;
import org.chromium.components.browser_ui.settings.EmbeddableSettingsPage;
import org.chromium.components.browser_ui.settings.SettingsFragment;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData.Entry;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.regional_capabilities.RegionalCapabilitiesService;

/**
 * A preference fragment for selecting a default search engine. ATTENTION: User can't change search
 * engine if it is controlled by an enterprise policy. Check
 * TemplateUrlServiceFactory.get().isDefaultSearchManaged() before launching this fragment.
 *
 * <p>TODO(crbug.com/41473490): Add on scroll shadow to action bar.
 */
@NullMarked
public class SearchEngineSettings extends ListFragment
        implements EmbeddableSettingsPage, ProfileDependentSetting {
    private static final String PREF_KEY_MANAGE_SEARCH_ENGINES_AND_SITE_SEARCH =
            "manage_search_engines_and_site_search";
    private SearchEngineAdapter mSearchEngineAdapter;
    private @Nullable Profile mProfile;
    private final SettableMonotonicObservableSupplier<String> mPageTitle =
            ObservableSuppliers.createMonotonic();

    String getValueForTesting() {
        return mSearchEngineAdapter.getValueForTesting();
    }

    String setValueForTesting(String value) {
        return mSearchEngineAdapter.setValueForTesting(value);
    }

    String getKeywordFromIndexForTesting(int index) {
        return mSearchEngineAdapter.getKeywordForTesting(index);
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mPageTitle.set(getString(R.string.search_engine_settings));
        createAdapterIfNecessary();
        setListAdapter(mSearchEngineAdapter);
    }

    @Override
    public MonotonicObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public void onViewCreated(View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        ListView listView = getListView();
        listView.setDivider(null);
        listView.setItemsCanFocus(true);

        assert mProfile != null;
        RegionalCapabilitiesService regionalCapabilities =
                RegionalCapabilitiesServiceFactory.getForProfile(mProfile);
        if (regionalCapabilities.isInEeaCountry()) {
            View headerView =
                    getLayoutInflater()
                            .inflate(R.layout.search_engine_choice_header, listView, false);
            listView.addHeaderView(headerView);
        }
    }

    @Override
    public void onStart() {
        super.onStart();
        mSearchEngineAdapter.start();
    }

    @Override
    public void onStop() {
        super.onStop();
        mSearchEngineAdapter.stop();
    }

    /**
     * Sets a runnable that disables auto search engine switching.
     * @param runnable Runnable to disable auto search engine switching.
     */
    public void setDisableAutoSwitchRunnable(Runnable runnable) {
        createAdapterIfNecessary();
        mSearchEngineAdapter.setDisableAutoSwitchRunnable(runnable);
    }

    private void createAdapterIfNecessary() {
        if (mSearchEngineAdapter != null) return;
        assert mProfile != null;
        Runnable siteSearchClickHandler =
                OmniboxFeatures.sOmniboxSiteSearch.isEnabled()
                        ? this::openSiteSearchSettings
                        : null;
        mSearchEngineAdapter =
                new SearchEngineAdapter(getActivity(), mProfile, siteSearchClickHandler);
    }

    @Override
    public void setProfile(Profile profile) {
        mProfile = profile;
    }

    public void overrideSearchEngineAdapterForTesting(SearchEngineAdapter searchEngineAdapter) {
        mSearchEngineAdapter = searchEngineAdapter;
    }

    @Override
    public @SettingsFragment.AnimationType int getAnimationType() {
        return SettingsFragment.AnimationType.PROPERTY;
    }

    @Override
    public @Nullable String getMainMenuKey() {
        return "search_engine";
    }

    private void openSiteSearchSettings() {
        SettingsNavigationFactory.createSettingsNavigation()
                .startSettings(
                        getContext(),
                        SiteSearchSettings.class,
                        /* fragmentArgs= */ null,
                        /* addToBackStack= */ true);
    }

    public static final ChromeBaseSearchIndexProvider SEARCH_INDEX_DATA_PROVIDER =
            new ChromeBaseSearchIndexProvider(SearchEngineSettings.class.getName(), 0) {
                @Override
                public void updateDynamicPreferences(Context context, SettingsIndexData indexData) {
                    if (!OmniboxFeatures.sOmniboxSiteSearch.isEnabled()) {
                        return;
                    }
                    String uniqueId = getUniqueId(PREF_KEY_MANAGE_SEARCH_ENGINES_AND_SITE_SEARCH);
                    Entry entry =
                            new Entry.Builder(
                                            uniqueId,
                                            PREF_KEY_MANAGE_SEARCH_ENGINES_AND_SITE_SEARCH,
                                            context.getString(
                                                    R.string.manage_search_engines_and_site_search),
                                            SearchEngineSettings.class.getName())
                                    .build();
                    indexData.addEntry(uniqueId, entry);
                }
            };
}
