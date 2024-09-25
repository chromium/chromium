// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings;

import android.os.Bundle;
import android.view.View;
import android.widget.ListView;

import androidx.fragment.app.ListFragment;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.settings.ProfileDependentSetting;
import org.chromium.components.browser_ui.settings.EmbeddableSettingsPage;
import org.chromium.components.search_engines.TemplateUrlService;

/**
 * A preference fragment for selecting a default search engine. ATTENTION: User can't change search
 * engine if it is controlled by an enterprise policy. Check
 * TemplateUrlServiceFactory.get().isDefaultSearchManaged() before launching this fragment.
 *
 * <p>TODO(crbug.com/41473490): Add on scroll shadow to action bar.
 */
public class SearchEngineSettings extends ListFragment
        implements EmbeddableSettingsPage, ProfileDependentSetting {
    private SearchEngineAdapter mSearchEngineAdapter;
    private Profile mProfile;
    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

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
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mPageTitle.set(getString(R.string.search_engine_settings));
        createAdapterIfNecessary();
        setListAdapter(mSearchEngineAdapter);
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        ListView listView = getListView();
        listView.setDivider(null);
        listView.setItemsCanFocus(true);

        TemplateUrlService templateUrlService = TemplateUrlServiceFactory.getForProfile(mProfile);
        if (templateUrlService.isEeaChoiceCountry()) {
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
        mSearchEngineAdapter = new SearchEngineAdapter(getActivity(), mProfile);
    }

    @Override
    public void setProfile(Profile profile) {
        mProfile = profile;
    }

    public void overrideSearchEngineAdapterForTesting(SearchEngineAdapter searchEngineAdapter) {
        mSearchEngineAdapter = searchEngineAdapter;
    }
}
