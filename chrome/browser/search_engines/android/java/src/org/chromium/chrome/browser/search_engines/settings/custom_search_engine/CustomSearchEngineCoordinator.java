// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.custom_search_engine;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.search_engines.settings.common.SearchEngineListPreference;
import org.chromium.chrome.browser.search_engines.settings.common.SiteSearchProperties;
import org.chromium.chrome.browser.search_engines.settings.common.SiteSearchViewBinder;
import org.chromium.chrome.browser.search_engines.settings.dialog.SiteSearchDialogCoordinator;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

@NullMarked
public class CustomSearchEngineCoordinator {
    private final ModelList mModelList = new ModelList();
    private final SimpleRecyclerViewAdapter mAdapter;
    private final CustomSearchEngineMediator mMediator;

    private final PropertyModel mModel;
    private final PropertyModelChangeProcessor mPropertyModelChangeProcessor;

    private final Context mContext;
    private final ModalDialogManager mModalDialogManager;
    private final SiteSearchDialogCoordinator mSiteSearchDialogCoordinator;

    public CustomSearchEngineCoordinator(
            Context context,
            Profile profile,
            SearchEngineListPreference pref,
            ModalDialogManager modalDialogManager) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mSiteSearchDialogCoordinator =
                new SiteSearchDialogCoordinator(
                        mContext,
                        mModalDialogManager,
                        TemplateUrlServiceFactory.getForProfile(profile));

        mAdapter = new SimpleRecyclerViewAdapter(mModelList);
        mAdapter.registerType(
                SiteSearchProperties.ViewType.SEARCH_ENGINE,
                parent -> {
                    View view =
                            LayoutInflater.from(parent.getContext())
                                    .inflate(R.layout.site_search_engine_item, parent, false);
                    view.setTag(new SiteSearchViewBinder.ViewHolder(view));
                    return view;
                },
                SiteSearchViewBinder::bind);

        mMediator =
                new CustomSearchEngineMediator(
                        context,
                        mModelList,
                        profile,
                        /* onEditSearchEngine= */ mSiteSearchDialogCoordinator::showEditDialog,
                        /* onRemoveSearchEngine= */ mSiteSearchDialogCoordinator
                                ::removeTemplateUrl);

        mModel =
                new PropertyModel.Builder(SiteSearchProperties.ALL_KEYS)
                        .with(SiteSearchProperties.ADAPTER, mAdapter)
                        .build();

        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, pref, SiteSearchViewBinder::bindPreference);
    }

    public void destroy() {
        mSiteSearchDialogCoordinator.dismiss();
        mPropertyModelChangeProcessor.destroy();
        mModel.set(SiteSearchProperties.ADAPTER, null);
        mMediator.destroy();
        mAdapter.destroy();
    }
}
