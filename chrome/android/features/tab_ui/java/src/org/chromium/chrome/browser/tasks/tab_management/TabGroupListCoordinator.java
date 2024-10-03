// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabGroupListProperties.ON_IS_SCROLLED_CHANGED;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.util.Consumer;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.data_sharing.DataSharingServiceFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.hub.PaneManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeControllerFactory;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgePadAdjuster;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeUtils;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper.FaviconImageCallback;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.MVCListAdapter.ViewBuilder;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Orchestrates the displaying of a list of interactable tab groups. */
public class TabGroupListCoordinator {
    @IntDef({RowType.TAB_GROUP})
    @Retention(RetentionPolicy.SOURCE)
    public @interface RowType {
        int TAB_GROUP = 0;
    }

    private final TabGroupListView mView;

    private final SimpleRecyclerViewAdapter mSimpleRecyclerViewAdapter;
    private TabGroupListMediator mTabGroupListMediator;
    private @Nullable EdgeToEdgePadAdjuster mEdgeToEdgePadAdjuster;

    /**
     * @param context Used to load resources and views.
     * @param filter Used to interact with local tab model and groups.
     * @param profileProvider Used to fetch keyed service.
     * @param paneManager Used to switch to show detailed tab group UI.
     * @param tabGroupUiActionHandler Used to open hidden tab groups.
     * @param modalDialogManager Used to show confirmation dialogs.
     * @param onIsScrolledChanged To be invoked whenever the scrolled state changes.
     * @param edgeToEdgeSupplier Supplier to the {@link EdgeToEdgeController} instance.
     */
    public TabGroupListCoordinator(
            Context context,
            TabGroupModelFilter filter,
            ProfileProvider profileProvider,
            PaneManager paneManager,
            TabGroupUiActionHandler tabGroupUiActionHandler,
            ModalDialogManager modalDialogManager,
            Consumer<Boolean> onIsScrolledChanged,
            @NonNull ObservableSupplier<EdgeToEdgeController> edgeToEdgeSupplier) {
        ModelList modelList = new ModelList();
        PropertyModel.Builder builder = new PropertyModel.Builder(TabGroupListProperties.ALL_KEYS);
        builder.with(ON_IS_SCROLLED_CHANGED, onIsScrolledChanged);
        PropertyModel propertyModel = builder.build();

        ViewBuilder<TabGroupRowView> layoutBuilder =
                new LayoutViewBuilder<>(R.layout.tab_group_row);
        mSimpleRecyclerViewAdapter =
                new SimpleRecyclerViewAdapter(modelList) {
                    @Override
                    public void onViewRecycled(SimpleRecyclerViewAdapter.ViewHolder holder) {
                        if (holder.getItemViewType() == RowType.TAB_GROUP) {
                            TabGroupRowViewBinder.onViewRecycled((TabGroupRowView) holder.itemView);
                        }
                        super.onViewRecycled(holder);
                    }
                };
        mSimpleRecyclerViewAdapter.registerType(
                RowType.TAB_GROUP, layoutBuilder, TabGroupRowViewBinder::bind);

        mView =
                (TabGroupListView)
                        LayoutInflater.from(context)
                                .inflate(R.layout.tab_group_list, /* root= */ null);
        PropertyModelChangeProcessor.create(propertyModel, mView, TabGroupListViewBinder::bind);
        mView.setRecyclerViewAdapter(mSimpleRecyclerViewAdapter);

        Profile profile = profileProvider.getOriginalProfile();
        FaviconResolver faviconResolver = buildFaviconResolver(context, profile);
        @Nullable TabGroupSyncService tabGroupSyncService = null;
        if (TabGroupSyncFeatures.isTabGroupSyncEnabled(profile)) {
            tabGroupSyncService = TabGroupSyncServiceFactory.getForProfile(profile);
        }

        @Nullable
        DataSharingService dataSharingService =
                ChromeFeatureList.isEnabled(ChromeFeatureList.DATA_SHARING)
                        ? DataSharingServiceFactory.getForProfile(profile)
                        : null;

        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(profile);
        ActionConfirmationManager actionConfirmationManager =
                new ActionConfirmationManager(profile, context, filter, modalDialogManager);
        SyncService syncService = SyncServiceFactory.getForProfile(profile);

        mTabGroupListMediator =
                new TabGroupListMediator(
                        context,
                        modelList,
                        propertyModel,
                        filter,
                        faviconResolver,
                        tabGroupSyncService,
                        dataSharingService,
                        identityManager,
                        paneManager,
                        tabGroupUiActionHandler,
                        actionConfirmationManager,
                        syncService,
                        modalDialogManager);

        if (EdgeToEdgeUtils.isDrawKeyNativePageToEdgeEnabled()) {
            mEdgeToEdgePadAdjuster =
                    EdgeToEdgeControllerFactory.createForViewAndObserveSupplier(
                            mView.getRecyclerView(), edgeToEdgeSupplier);
        }
    }

    @VisibleForTesting
    static FaviconResolver buildFaviconResolver(Context context, Profile profile) {
        TabListFaviconProvider fallbackProvider =
                new TabListFaviconProvider(
                        context, /* isTabStrip= */ false, R.dimen.default_favicon_corner_radius);
        return (GURL url, Callback<Drawable> callback) -> {
            if (UrlUtilities.isInternalScheme(url)) {
                callback.onResult(
                        fallbackProvider
                                .getRoundedChromeFavicon(/* isIncognito= */ false)
                                .getDefaultDrawable());
            } else {
                resolveForeignFavicon(context, profile, fallbackProvider, url, callback);
            }
        };
    }

    private static void resolveForeignFavicon(
            Context context,
            Profile profile,
            TabListFaviconProvider fallbackProvider,
            GURL url,
            Callback<Drawable> callback) {
        Resources resources = context.getResources();
        int faviconSizePixels = resources.getDimensionPixelSize(R.dimen.tab_grid_favicon_size);
        FaviconImageCallback faviconImageCallback =
                (Bitmap bitmap, GURL ignored) ->
                        onForeignFavicon(context, fallbackProvider, callback, bitmap);
        new FaviconHelper()
                .getForeignFaviconImageForURL(
                        profile, url, faviconSizePixels, faviconImageCallback);
    }

    private static void onForeignFavicon(
            Context context,
            TabListFaviconProvider fallbackProvider,
            Callback<Drawable> callback,
            Bitmap bitmap) {
        Resources resources = context.getResources();
        final Drawable drawable;
        if (bitmap == null) {
            drawable =
                    fallbackProvider
                            .getDefaultFavicon(/* isIncognito= */ false)
                            .getDefaultDrawable();
        } else {
            int cornerRadiusPixels =
                    resources.getDimensionPixelSize(R.dimen.default_favicon_corner_radius);
            drawable = ViewUtils.createRoundedBitmapDrawable(resources, bitmap, cornerRadiusPixels);
        }
        callback.onResult(drawable);
    }

    /** Returns the root view of this component, allowing the parent to anchor in the hierarchy. */
    public View getView() {
        return mView;
    }

    /** Permanently cleans up this component. */
    public void destroy() {
        mTabGroupListMediator.destroy();
        mSimpleRecyclerViewAdapter.destroy();
        if (mEdgeToEdgePadAdjuster != null) {
            mEdgeToEdgePadAdjuster.destroy();
            mEdgeToEdgePadAdjuster = null;
        }
    }
}
