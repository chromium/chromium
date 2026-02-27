// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.extensions;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.settings.common.SearchEngineListPreference;
import org.chromium.chrome.browser.search_engines.settings.common.SiteSearchProperties;
import org.chromium.chrome.browser.search_engines.settings.common.SiteSearchViewBinder;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

@NullMarked
public class ExtensionSearchEngineCoordinator {
    private final ModelList mModelList;
    private final SimpleRecyclerViewAdapter mAdapter;
    private final ExtensionSearchEngineMediator mMediator;
    private final PropertyModel mPropertyModel;
    private final PropertyModelChangeProcessor mPropertyModelChangeProcessor;

    public ExtensionSearchEngineCoordinator(
            Context context, Profile profile, SearchEngineListPreference pref) {
        mModelList = new ModelList();
        mAdapter = new ExtensionSearchEngineAdapter(context, mModelList);
        mMediator = new ExtensionSearchEngineMediator(context, mModelList, profile);

        mPropertyModel =
                new PropertyModel.Builder(SiteSearchProperties.ALL_KEYS)
                        .with(SiteSearchProperties.ADAPTER, mAdapter)
                        .build();

        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mPropertyModel, pref, SiteSearchViewBinder::bindPreference);
    }

    public void destroy() {
        mPropertyModel.set(SiteSearchProperties.ADAPTER, null);
        mPropertyModelChangeProcessor.destroy();
        mAdapter.destroy();
        mMediator.destroy();
    }
}
