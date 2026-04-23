// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.custom_site_search;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.search_engines.settings.common.SearchEngineListPreference;
import org.chromium.chrome.browser.search_engines.settings.common.SiteSearchProperties;
import org.chromium.chrome.browser.search_engines.settings.common.SiteSearchViewBinder;
import org.chromium.chrome.browser.search_engines.settings.dialog.SiteSearchDialogCoordinator;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.ListObservable;
import org.chromium.ui.modelutil.ListObservable.ListObserver;
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
    private final SiteSearchDialogCoordinator mSiteSearchDialogCoordinator;
    private final ListObserver<Void> mListObserver;

    public CustomSiteSearchCoordinator(
            Context context,
            Profile profile,
            SearchEngineListPreference pref,
            ModalDialogManager modalDialogManager) {
        mModelList = new ModelList();
        mAdapter = new CustomSiteSearchAdapter(context, mModelList);

        mSiteSearchDialogCoordinator =
                new SiteSearchDialogCoordinator(
                        context,
                        modalDialogManager,
                        TemplateUrlServiceFactory.getForProfile(profile));

        mMediator =
                new CustomSiteSearchMediator(
                        context,
                        mModelList,
                        profile,
                        /* onAddSearchEngine= */ mSiteSearchDialogCoordinator::showAddDialog,
                        /* onEditSearchEngine= */ mSiteSearchDialogCoordinator::showEditDialog,
                        /* onRemoveSearchEngine= */ mSiteSearchDialogCoordinator
                                ::removeTemplateUrl);

        mListObserver =
                new ListObserver<Void>() {
                    @Override
                    public void onItemRangeInserted(ListObservable source, int index, int count) {
                        pref.invalidateDecorations();
                    }

                    @Override
                    public void onItemRangeRemoved(ListObservable source, int index, int count) {
                        pref.invalidateDecorations();
                    }
                };

        mModelList.addObserver(mListObserver);

        mPropertyModel =
                new PropertyModel.Builder(SiteSearchProperties.ALL_KEYS)
                        .with(SiteSearchProperties.ADAPTER, mAdapter)
                        .build();

        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mPropertyModel, pref, SiteSearchViewBinder::bindPreference);
    }

    public void destroy() {
        mSiteSearchDialogCoordinator.dismiss();
        mPropertyModel.set(SiteSearchProperties.ADAPTER, null);
        mPropertyModelChangeProcessor.destroy();
        mModelList.removeObserver(mListObserver);
        mAdapter.destroy();
        mMediator.destroy();
    }
}
