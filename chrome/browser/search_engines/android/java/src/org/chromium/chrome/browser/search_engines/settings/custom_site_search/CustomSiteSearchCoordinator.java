// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.custom_site_search;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

@NullMarked
public class CustomSiteSearchCoordinator {
    private final ModelList mModelList;
    private final SimpleRecyclerViewAdapter mAdapter;
    private final CustomSiteSearchMediator mMediator;
    private final PropertyModel mPropertyModel;
    private final PropertyModelChangeProcessor mPropertyModelChangeProcessor;

    public CustomSiteSearchCoordinator(
            Context context, Profile profile, CustomSiteSearchListPreference pref) {
        mModelList = new ModelList();
        mAdapter = new CustomSiteSearchAdapter(context, mModelList);
        mMediator = new CustomSiteSearchMediator(context, mModelList, profile);

        mPropertyModel =
                new PropertyModel.Builder(CustomSiteSearchProperties.ALL_KEYS)
                        .with(CustomSiteSearchProperties.ADAPTER, mAdapter)
                        .build();

        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mPropertyModel, pref, CustomSiteSearchViewBinder::bindPreference);
    }

    public void destroy() {
        mPropertyModel.set(CustomSiteSearchProperties.ADAPTER, null);
        mPropertyModelChangeProcessor.destroy();
        mAdapter.destroy();
        mMediator.destroy();
    }
}
