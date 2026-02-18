// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.custom_search_engine;

import android.content.Context;
import android.view.LayoutInflater;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.chrome.browser.search_engines.settings.custom_search_engine.CustomSearchEngineProperties.CustomSearchEngineRecyclerViewItems;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

@NullMarked
public class CustomSearchEngineListCoordinator {
    private final ModelList mModelList = new ModelList();
    private final SimpleRecyclerViewAdapter mAdapter;
    private final CustomSearchEngineListMediator mMediator;

    private final PropertyModel mModel;
    private final PropertyModelChangeProcessor mPropertyModelChangeProcessor;

    public CustomSearchEngineListCoordinator(
            Context context, Profile profile, CustomSearchEngineListPreference pref) {
        mAdapter = new SimpleRecyclerViewAdapter(mModelList);
        mAdapter.registerType(
                CustomSearchEngineRecyclerViewItems.DEFAULT,
                parent ->
                        LayoutInflater.from(parent.getContext())
                                .inflate(R.layout.custom_search_engine_item, parent, false),
                CustomSearchEngineViewBinder::bind);
        mMediator = new CustomSearchEngineListMediator(context, mModelList, profile);

        mModel =
                new PropertyModel.Builder(CustomSearchEngineProperties.ALL_KEYS)
                        .with(CustomSearchEngineProperties.ADAPTER, mAdapter)
                        .build();

        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, pref, CustomSearchEngineViewBinder::bindPreference);
    }

    public void destroy() {
        mModel.set(CustomSearchEngineProperties.ADAPTER, null);
        mPropertyModelChangeProcessor.destroy();
        mMediator.destroy();
        mAdapter.destroy();
    }
}
