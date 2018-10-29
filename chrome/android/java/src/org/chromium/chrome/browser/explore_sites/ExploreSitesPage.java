// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import android.content.Context;
import android.support.annotation.IntDef;
import android.support.annotation.Nullable;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.modelutil.ListModel;
import org.chromium.chrome.browser.modelutil.PropertyModel;
import org.chromium.chrome.browser.modelutil.RecyclerViewAdapter;
import org.chromium.chrome.browser.native_page.BasicNativePage;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.native_page.NativePageHost;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegateImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.widget.RoundedIconGenerator;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.List;

/**
 * Provides functionality when the user interacts with the explore sites page.
 */
public class ExploreSitesPage extends BasicNativePage {
    private static final String TAG = "ExploreSitesPage";
    private static final String CONTEXT_MENU_USER_ACTION_PREFIX = "ExploreSites";
    private static final int INITIAL_SCROLL_POSITION = 3;
    static final PropertyModel.WritableIntPropertyKey STATUS_KEY =
            new PropertyModel.WritableIntPropertyKey();
    static final PropertyModel.WritableIntPropertyKey SCROLL_TO_CATEGORY_KEY =
            new PropertyModel.WritableIntPropertyKey();
    static final PropertyModel
            .ReadableObjectPropertyKey<ListModel<ExploreSitesCategory>> CATEGORY_LIST_KEY =
            new PropertyModel.ReadableObjectPropertyKey<>();

    @IntDef({CatalogLoadingState.LOADING, CatalogLoadingState.SUCCESS, CatalogLoadingState.ERROR})
    @Retention(RetentionPolicy.SOURCE)
    public @interface CatalogLoadingState {
        int LOADING = 1; // Loading catalog info from disk.
        int SUCCESS = 2;
        int ERROR = 3; // Error retrieving catalog resources from internet.
        int LOADING_NET = 4; // Retrieving catalog resources from internet.
    }

    private TabModelSelector mTabModelSelector;
    private NativePageHost mHost;
    private Profile mProfile;
    private ViewGroup mView;
    private String mTitle;
    private PropertyModel mModel;
    private ContextMenuManager mContextMenuManager;
    private String mNavFragment;
    private boolean mHasFetchedNetworkCatalog;

    /**
     * Create a new instance of the explore sites page.
     */
    public ExploreSitesPage(ChromeActivity activity, NativePageHost host) {
        super(activity, host);
    }

    @Override
    protected void initialize(ChromeActivity activity, final NativePageHost host) {
        mHost = host;
        mTabModelSelector = activity.getTabModelSelector();
        mTitle = activity.getString(R.string.explore_sites_title);
        mView = (ViewGroup) activity.getLayoutInflater().inflate(
                R.layout.explore_sites_page_layout, null);
        mProfile = mHost.getActiveTab().getProfile();
        mHasFetchedNetworkCatalog = false;

        mModel = new PropertyModel.Builder(STATUS_KEY, SCROLL_TO_CATEGORY_KEY, CATEGORY_LIST_KEY)
                         .with(CATEGORY_LIST_KEY, new ListModel<ExploreSitesCategory>())
                         .with(SCROLL_TO_CATEGORY_KEY, 0)
                         .with(STATUS_KEY, CatalogLoadingState.LOADING)
                         .build();

        Context context = mView.getContext();
        LinearLayoutManager layoutManager = new LinearLayoutManager(context);
        int iconSizePx = context.getResources().getDimensionPixelSize(R.dimen.tile_view_icon_size);
        RoundedIconGenerator iconGenerator = new RoundedIconGenerator(iconSizePx, iconSizePx,
                iconSizePx / 2,
                ApiCompatibilityUtils.getColor(
                        context.getResources(), R.color.default_favicon_background_color),
                context.getResources().getDimensionPixelSize(R.dimen.tile_view_icon_text_size));

        NativePageNavigationDelegateImpl navDelegate =
                new NativePageNavigationDelegateImpl(activity, mProfile, host, mTabModelSelector);
        // Don't direct reference activity because it might change if tab is reparented.
        Runnable closeContextMenuCallback =
                () -> host.getActiveTab().getActivity().closeContextMenu();
        mContextMenuManager = new ContextMenuManager(navDelegate, this::setTouchEnabled,
                closeContextMenuCallback, CONTEXT_MENU_USER_ACTION_PREFIX);
        host.getActiveTab().getWindowAndroid().addContextMenuCloseListener(mContextMenuManager);

        CategoryCardAdapter adapterDelegate = new CategoryCardAdapter(
                mModel, layoutManager, iconGenerator, mContextMenuManager, navDelegate, mProfile);

        RecyclerView recyclerView =
                (RecyclerView) mView.findViewById(R.id.explore_sites_category_recycler);
        RecyclerViewAdapter<CategoryCardViewHolderFactory.CategoryCardViewHolder, Void> adapter =
                new RecyclerViewAdapter<>(adapterDelegate, new CategoryCardViewHolderFactory());

        recyclerView.setLayoutManager(layoutManager);
        recyclerView.setAdapter(adapter);

        ExploreSitesBridge.getEspCatalog(mProfile, this::translateToModel);
        RecordUserAction.record("Android.ExploreSitesPage.Open");

        // TODO(chili): Set layout to be an observer of list model
    }

    private void translateToModel(@Nullable List<ExploreSitesCategory> categoryList) {
        // If list is null or we received an empty catalog from network, show error.
        if (categoryList == null || (categoryList.isEmpty() && mHasFetchedNetworkCatalog)) {
            mModel.set(STATUS_KEY, CatalogLoadingState.ERROR);
            return;
        }
        // If list is empty and we never fetched from network before, fetch from network.
        if (categoryList.isEmpty()) {
            mModel.set(STATUS_KEY, CatalogLoadingState.LOADING_NET);
            mHasFetchedNetworkCatalog = true;
            ExploreSitesBridge.updateCatalogFromNetwork(
                    mProfile, /* isImmediateFetch =*/true, this::onUpdatedCatalog);
            return;
        }
        mModel.set(STATUS_KEY, CatalogLoadingState.SUCCESS);
        ListModel<ExploreSitesCategory> categoryListModel = mModel.get(CATEGORY_LIST_KEY);
        categoryListModel.set(categoryList);
        if (mNavFragment != null) {
            lookupCategoryAndScroll(mNavFragment);
        } else {
            mModel.set(SCROLL_TO_CATEGORY_KEY,
                    Math.min(categoryListModel.size() - 1, INITIAL_SCROLL_POSITION));
        }
    }

    private void onUpdatedCatalog(Boolean hasFetchedCatalog) {
        if (hasFetchedCatalog) {
            ExploreSitesBridge.getEspCatalog(mProfile, this::translateToModel);
        } else {
            mModel.set(STATUS_KEY, CatalogLoadingState.ERROR);
        }
    }

    @Override
    public String getHost() {
        return UrlConstants.EXPLORE_HOST;
    }

    @Override
    public View getView() {
        return mView;
    }

    @Override
    public String getTitle() {
        return mTitle;
    }

    @Override
    public void updateForUrl(String url) {
        super.updateForUrl(url);
        String fragment;
        try {
            fragment = new URI(url).getFragment();
        } catch (URISyntaxException e) {
            fragment = "";
        }
        if (mModel.get(STATUS_KEY) != CatalogLoadingState.SUCCESS) {
            mNavFragment = fragment;
        } else {
            lookupCategoryAndScroll(fragment);
        }
    }

    @Override
    public void destroy() {
        mHost.getActiveTab().getWindowAndroid().removeContextMenuCloseListener(mContextMenuManager);
        super.destroy();
    }

    private void setTouchEnabled(boolean enabled) {} // Does nothing.

    private void lookupCategoryAndScroll(String categoryId) {
        int position = 0;
        try {
            int id = Integer.parseInt(categoryId);
            ListModel<ExploreSitesCategory> categoryList = mModel.get(CATEGORY_LIST_KEY);
            for (int i = 0; i < categoryList.size(); i++) {
                if (categoryList.get(i).getId() == id) {
                    position = i;
                    break;
                }
            }
        } catch (NumberFormatException e) {
        } // do nothing
        mModel.set(SCROLL_TO_CATEGORY_KEY, position);
        mNavFragment = null;
    }
}
