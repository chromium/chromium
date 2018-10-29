// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import android.support.annotation.IntDef;
import android.support.annotation.Nullable;
import android.support.v7.widget.RecyclerView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.modelutil.ListObservable;
import org.chromium.chrome.browser.modelutil.ListObservable.ListObserver;
import org.chromium.chrome.browser.modelutil.ListObservableImpl;
import org.chromium.chrome.browser.modelutil.PropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModel;
import org.chromium.chrome.browser.modelutil.PropertyObservable;
import org.chromium.chrome.browser.modelutil.RecyclerViewAdapter;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegate;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.widget.LoadingView;
import org.chromium.chrome.browser.widget.RoundedIconGenerator;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Recycler view adapter delegate for a model containing a list of category cards and an error code.
 */
class CategoryCardAdapter extends ListObservableImpl<Void>
        implements RecyclerViewAdapter
                           .Delegate<CategoryCardViewHolderFactory.CategoryCardViewHolder, Void>,
                   PropertyObservable.PropertyObserver<PropertyKey>, ListObserver<Void> {
    @IntDef({ViewType.HEADER, ViewType.CATEGORY, ViewType.LOADING, ViewType.ERROR})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ViewType {
        int HEADER = 0;
        int CATEGORY = 1;
        int LOADING = 2;
        int ERROR = 3;
    }

    private final RoundedIconGenerator mIconGenerator;
    private final ContextMenuManager mContextMenuManager;
    private final NativePageNavigationDelegate mNavDelegate;
    private final Profile mProfile;

    private RecyclerView.LayoutManager mLayoutManager;
    private PropertyModel mCategoryModel;

    public CategoryCardAdapter(PropertyModel model, RecyclerView.LayoutManager layoutManager,
            RoundedIconGenerator iconGenerator, ContextMenuManager contextMenuManager,
            NativePageNavigationDelegate navDelegate, Profile profile) {
        mCategoryModel = model;
        mCategoryModel.addObserver(this);
        mCategoryModel.get(ExploreSitesPage.CATEGORY_LIST_KEY).addObserver(this);

        mLayoutManager = layoutManager;
        mIconGenerator = iconGenerator;
        mContextMenuManager = contextMenuManager;
        mNavDelegate = navDelegate;
        mProfile = profile;
    }

    @Override
    public int getItemCount() {
        // If not loaded, return 2 for title and error/loading section
        if (mCategoryModel.get(ExploreSitesPage.STATUS_KEY)
                != ExploreSitesPage.CatalogLoadingState.SUCCESS) {
            return 2;
        }
        // Add 1 for title.
        return mCategoryModel.get(ExploreSitesPage.CATEGORY_LIST_KEY).size() + 1;
    }

    @Override
    @ViewType
    public int getItemViewType(int position) {
        if (position == 0) return ViewType.HEADER;
        switch (mCategoryModel.get(ExploreSitesPage.STATUS_KEY)) {
            case ExploreSitesPage.CatalogLoadingState.ERROR:
                return ViewType.ERROR;
            case ExploreSitesPage.CatalogLoadingState.LOADING: // fall-through
            case ExploreSitesPage.CatalogLoadingState.LOADING_NET:
                return ViewType.LOADING;
            case ExploreSitesPage.CatalogLoadingState.SUCCESS:
                return ViewType.CATEGORY;
            default:
                assert(false);
                return ViewType.ERROR;
        }
    }

    @Override
    public void onBindViewHolder(CategoryCardViewHolderFactory.CategoryCardViewHolder holder,
            int position, @Nullable Void payload) {
        if (holder.getItemViewType() == ViewType.CATEGORY) {
            ExploreSitesCategoryCardView view = (ExploreSitesCategoryCardView) holder.itemView;
            // Position - 1 because there is always title.
            view.setCategory(
                    mCategoryModel.get(ExploreSitesPage.CATEGORY_LIST_KEY).get(position - 1),
                    position - 1, mIconGenerator, mContextMenuManager, mNavDelegate, mProfile);
        } else if (holder.getItemViewType() == ViewType.LOADING) {
            // Start spinner.
            LoadingView spinner = holder.itemView.findViewById(R.id.loading);
            spinner.showLoadingUI();
        }
    }

    @Override
    public void onPropertyChanged(
            PropertyObservable<PropertyKey> source, @Nullable PropertyKey key) {
        if (key == ExploreSitesPage.STATUS_KEY) {
            int status = mCategoryModel.get(ExploreSitesPage.STATUS_KEY);
            if (status != ExploreSitesPage.CatalogLoadingState.SUCCESS) {
                notifyItemChanged(1);
            } else {
                // If it's success, we must "remove" the loading view.
                // Loading the categories is taken care of by the list observer.
                notifyItemRangeRemoved(/* index= */ 1, /* count= */ 1);
            }
        }
        if (key == ExploreSitesPage.SCROLL_TO_CATEGORY_KEY) {
            int pos = mCategoryModel.get(ExploreSitesPage.SCROLL_TO_CATEGORY_KEY);
            // Add 1 for title.
            mLayoutManager.scrollToPosition(pos + 1);
        }
    }

    @Override
    public void onItemRangeInserted(ListObservable source, int index, int count) {
        // Add 1 because of title.
        notifyItemRangeInserted(index + 1, count);
    }

    @Override
    public void onItemRangeRemoved(ListObservable source, int index, int count) {
        // Add 1 because of title.
        notifyItemRangeRemoved(index + 1, count);
    }

    @Override
    public void onItemRangeChanged(
            ListObservable<Void> source, int index, int count, @Nullable Void payload) {
        // Add 1 because of title.
        notifyItemRangeChanged(index + 1, count, payload);
    }
}
