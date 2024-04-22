// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper.FaviconImageCallback;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.MVCListAdapter.ViewBuilder;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.function.BiConsumer;

/** Orchestrates the displaying of a list of interactable tab groups. */
public class TabGroupListCoordinator {
    @IntDef({RowType.TAB_GROUP})
    @Retention(RetentionPolicy.SOURCE)
    public @interface RowType {
        int TAB_GROUP = 0;
    }

    private final RecyclerView mRecyclerView;
    private final SimpleRecyclerViewAdapter mSimpleRecyclerViewAdapter;
    private TabGroupListMediator mTabGroupListMediator;

    public TabGroupListCoordinator(
            Context context, TabGroupModelFilter filter, ProfileProvider profileProvider) {
        ModelList modelList = new ModelList();

        ViewBuilder<TabGroupRowView> layoutBuilder =
                new LayoutViewBuilder<>(R.layout.tab_group_row);
        mSimpleRecyclerViewAdapter = new SimpleRecyclerViewAdapter(modelList);
        mSimpleRecyclerViewAdapter.registerType(
                RowType.TAB_GROUP, layoutBuilder, new TabGroupRowViewBinder());

        mRecyclerView = new RecyclerView(context);
        mRecyclerView.setLayoutManager(new LinearLayoutManager(context));
        mRecyclerView.setAdapter(mSimpleRecyclerViewAdapter);
        mRecyclerView.setItemAnimator(null);

        Resources resources = context.getResources();
        Profile profile = profileProvider.getOriginalProfile();
        BiConsumer<GURL, Callback<Drawable>> faviconResolver =
                buildFaviconResolver(resources, profile);
        TabGroupSyncService syncService = TabGroupSyncServiceFactory.getForProfile(profile);
        mTabGroupListMediator =
                new TabGroupListMediator(modelList, filter, faviconResolver, syncService);
    }

    private BiConsumer<GURL, Callback<Drawable>> buildFaviconResolver(
            Resources resources, Profile profile) {
        int faviconSizePixels = resources.getDimensionPixelSize(R.dimen.tab_grid_favicon_size);
        int cornerRadiusPixels =
                resources.getDimensionPixelSize(R.dimen.default_favicon_corner_radius);
        FaviconHelper faviconHelper = new FaviconHelper();
        return (GURL url, Callback<Drawable> callback) -> {
            FaviconImageCallback faviconImageCallback =
                    (Bitmap bitmap, GURL ignored) -> {
                        Drawable drawable =
                                ViewUtils.createRoundedBitmapDrawable(
                                        resources, bitmap, cornerRadiusPixels);
                        callback.onResult(drawable);
                    };
            faviconHelper.getForeignFaviconImageForURL(
                    profile, url, faviconSizePixels, faviconImageCallback);
        };
    }

    /** Returns the root view of this component, allowing the parent to anchor in the hierarchy. */
    public View getView() {
        return mRecyclerView;
    }

    /** Permanently cleans up this component. */
    public void destroy() {
        mTabGroupListMediator.destroy();
        mSimpleRecyclerViewAdapter.destroy();
    }
}
