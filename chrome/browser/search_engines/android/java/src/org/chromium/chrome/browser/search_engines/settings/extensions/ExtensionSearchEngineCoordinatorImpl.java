// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.extensions;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.ServiceImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.settings.common.SearchEngineListPreference;
import org.chromium.chrome.browser.search_engines.settings.common.SiteSearchProperties;
import org.chromium.chrome.browser.search_engines.settings.common.SiteSearchViewBinder;
import org.chromium.components.browser_ui.settings.SettingsCustomTabLauncher;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/** Implementation of {@link ExtensionSearchEngineCoordinator}. */
@NullMarked
@ServiceImpl(ExtensionSearchEngineCoordinator.class)
public class ExtensionSearchEngineCoordinatorImpl implements ExtensionSearchEngineCoordinator {
    private ModelList mModelList;
    private SimpleRecyclerViewAdapter mAdapter;
    private ExtensionSearchEngineMediator mMediator;
    private PropertyModel mPropertyModel;
    private PropertyModelChangeProcessor mPropertyModelChangeProcessor;

    public ExtensionSearchEngineCoordinatorImpl() {}

    @Override
    public void initialize(
            Context context,
            Profile profile,
            SearchEngineListPreference pref,
            SettingsCustomTabLauncher settingsCustomTabLauncher) {
        mModelList = new ModelList();
        mAdapter = new ExtensionSearchEngineAdapter(context, mModelList);
        mMediator =
                new ExtensionSearchEngineMediator(
                        context, mModelList, profile, settingsCustomTabLauncher);

        mPropertyModel =
                new PropertyModel.Builder(SiteSearchProperties.ALL_KEYS)
                        .with(SiteSearchProperties.ADAPTER, mAdapter)
                        .build();

        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mPropertyModel, pref, SiteSearchViewBinder::bindPreference);
    }

    @Override
    public void destroy() {
        if (mPropertyModel != null) {
            // Clear the adapter first so the change processor can notify the preference to unbind
            // it before being destroyed.
            mPropertyModel.set(SiteSearchProperties.ADAPTER, null);
        }
        if (mPropertyModelChangeProcessor != null) {
            mPropertyModelChangeProcessor.destroy();
        }
        if (mMediator != null) {
            mMediator.destroy();
        }
        if (mAdapter != null) {
            mAdapter.destroy();
        }
    }
}
