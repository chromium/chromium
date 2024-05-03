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
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.hub.PaneManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper.FaviconImageCallback;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.modaldialog.ModalDialogManager;
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

    /**
     * @param context Used to load resources and views.
     * @param filter Used to interact with local tab model and groups.
     * @param profileProvider Used to fetch keyed service.
     * @param paneManager Used to switch to show detailed tab group UI.
     * @param tabGroupUiActionHandler Used to open hidden tab groups.
     * @param modalDialogManager Used to show confirmation dialogs.
     */
    public TabGroupListCoordinator(
            Context context,
            TabGroupModelFilter filter,
            ProfileProvider profileProvider,
            PaneManager paneManager,
            TabGroupUiActionHandler tabGroupUiActionHandler,
            ModalDialogManager modalDialogManager) {
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

        Profile profile = profileProvider.getOriginalProfile();
        BiConsumer<GURL, Callback<Drawable>> faviconResolver =
                buildFaviconResolver(context, profile);
        TabGroupSyncService syncService = TabGroupSyncServiceFactory.getForProfile(profile);
        ActionConfirmationManager actionConfirmationManager =
                new ActionConfirmationManager(profile, context, filter, modalDialogManager);
        mTabGroupListMediator =
                new TabGroupListMediator(
                        modelList,
                        filter,
                        faviconResolver,
                        syncService,
                        paneManager,
                        tabGroupUiActionHandler,
                        actionConfirmationManager);
    }

    @VisibleForTesting
    static BiConsumer<GURL, Callback<Drawable>> buildFaviconResolver(
            Context context, Profile profile) {
        return (GURL url, Callback<Drawable> callback) -> {
            if (UrlUtilities.isInternalScheme(url)) {
                // Do not bother resizing, the view will use a fixed size. No resizing should allow
                // caching/sharing of this drawable.
                callback.onResult(AppCompatResources.getDrawable(context, R.drawable.chromelogo16));
            } else {
                resolveForeignFavicon(context, profile, url, callback);
            }
        };
    }

    private static void resolveForeignFavicon(
            Context context, Profile profile, GURL url, Callback<Drawable> callback) {
        Resources resources = context.getResources();
        int faviconSizePixels = resources.getDimensionPixelSize(R.dimen.tab_grid_favicon_size);
        FaviconImageCallback faviconImageCallback =
                (Bitmap bitmap, GURL ignored) -> onForeignFavicon(context, callback, bitmap);
        new FaviconHelper()
                .getForeignFaviconImageForURL(
                        profile, url, faviconSizePixels, faviconImageCallback);
    }

    private static void onForeignFavicon(
            Context context, Callback<Drawable> callback, Bitmap bitmap) {
        Resources resources = context.getResources();
        final Drawable drawable;
        if (bitmap == null) {
            // Do not bother resizing, the view will use a fixed size. No resizing should allow
            // caching/sharing of this drawable.
            drawable = AppCompatResources.getDrawable(context, R.drawable.ic_globe_24dp);
        } else {
            int cornerRadiusPixels =
                    resources.getDimensionPixelSize(R.dimen.default_favicon_corner_radius);
            drawable = ViewUtils.createRoundedBitmapDrawable(resources, bitmap, cornerRadiusPixels);
        }
        callback.onResult(drawable);
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
