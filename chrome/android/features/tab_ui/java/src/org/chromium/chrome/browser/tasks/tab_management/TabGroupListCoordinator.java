// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabGroupListProperties.ENABLE_CONTAINMENT;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupListProperties.ON_IS_SCROLLED_CHANGED;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.util.Consumer;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.collaboration.messaging.MessagingBackendServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.hub.PaneManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.tab.TabFavicon;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tab_ui.ActionConfirmationManager;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.theme.SurfaceColorUpdateUtils;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeControllerFactory;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeUtils;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.edge_to_edge.EdgeToEdgePadAdjuster;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.collaboration.messaging.MessagingBackendService;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.sync.SyncService;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.MVCListAdapter.ViewBuilder;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Orchestrates the displaying of a list of interactable tab groups. */
public class TabGroupListCoordinator {
    @IntDef({RowType.TAB_GROUP, RowType.TAB_GROUP_REMOVED_CARD})
    @Retention(RetentionPolicy.SOURCE)
    public @interface RowType {
        int TAB_GROUP = 0;
        int TAB_GROUP_REMOVED_CARD = 1;
    }

    private final TabGroupListView mView;
    private final SimpleRecyclerViewAdapter mSimpleRecyclerViewAdapter;
    private final TabListFaviconProvider mTabListFaviconProvider;

    private final TabGroupListMediator mTabGroupListMediator;
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
     * @param dataSharingTabManager The {@link} DataSharingTabManager to start collaboration flows.
     */
    public TabGroupListCoordinator(
            Context context,
            TabGroupModelFilter filter,
            ProfileProvider profileProvider,
            PaneManager paneManager,
            TabGroupUiActionHandler tabGroupUiActionHandler,
            ModalDialogManager modalDialogManager,
            Consumer<Boolean> onIsScrolledChanged,
            @NonNull ObservableSupplier<EdgeToEdgeController> edgeToEdgeSupplier,
            @NonNull DataSharingTabManager dataSharingTabManager) {
        ModelList modelList = new ModelList();
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

        PropertyModel.Builder builder = new PropertyModel.Builder(TabGroupListProperties.ALL_KEYS);
        builder.with(ON_IS_SCROLLED_CHANGED, onIsScrolledChanged);
        builder.with(ENABLE_CONTAINMENT, enableContainment());
        PropertyModel propertyModel = builder.build();

        ViewBuilder<TabGroupRowView> innerBuilder = new LayoutViewBuilder<>(R.layout.tab_group_row);
        ViewBuilder<TabGroupRowView> tabGroupRowLayoutBuilder =
                new ViewBuilder<TabGroupRowView>() {
                    @Override
                    public TabGroupRowView buildView(ViewGroup parent) {
                        TabGroupRowView view = innerBuilder.buildView(parent);
                        if (enableContainment()) view.setupForContainment();
                        return view;
                    }
                };

        mSimpleRecyclerViewAdapter.registerType(
                RowType.TAB_GROUP, tabGroupRowLayoutBuilder, TabGroupRowViewBinder::bind);

        ViewBuilder<MessageCardView> tabGroupMessageCardLayoutBuilder =
                new LayoutViewBuilder<>(R.layout.tab_grid_message_card_item);
        mSimpleRecyclerViewAdapter.registerType(
                RowType.TAB_GROUP_REMOVED_CARD,
                tabGroupMessageCardLayoutBuilder,
                MessageCardViewBinder::bind);

        mView =
                (TabGroupListView)
                        LayoutInflater.from(context)
                                .inflate(R.layout.tab_group_list, /* root= */ null);
        PropertyModelChangeProcessor.create(propertyModel, mView, TabGroupListViewBinder::bind);
        mView.setRecyclerViewAdapter(mSimpleRecyclerViewAdapter);

        Profile profile = profileProvider.getOriginalProfile();
        mTabListFaviconProvider =
                new TabListFaviconProvider(
                        context,
                        /* isTabStrip= */ false,
                        R.dimen.default_favicon_corner_radius,
                        TabFavicon::getBitmap);
        FaviconResolver faviconResolver =
                TabGroupListFaviconResolverFactory.build(context, profile, mTabListFaviconProvider);
        @Nullable TabGroupSyncService tabGroupSyncService = null;
        if (TabGroupSyncFeatures.isTabGroupSyncEnabled(profile)) {
            tabGroupSyncService = TabGroupSyncServiceFactory.getForProfile(profile);
        }

        @NonNull
        CollaborationService collaborationService =
                CollaborationServiceFactory.getForProfile(profile);

        @NonNull
        DataSharingService dataSharingService = DataSharingServiceFactory.getForProfile(profile);

        @NonNull
        MessagingBackendService messagingBackendService =
                MessagingBackendServiceFactory.getForProfile(profile);

        ActionConfirmationManager actionConfirmationManager =
                new ActionConfirmationManager(profile, context, modalDialogManager);
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
                        collaborationService,
                        messagingBackendService,
                        paneManager,
                        tabGroupUiActionHandler,
                        actionConfirmationManager,
                        syncService,
                        enableContainment(),
                        dataSharingTabManager);

        if (EdgeToEdgeUtils.isDrawKeyNativePageToEdgeEnabled()) {
            mEdgeToEdgePadAdjuster =
                    EdgeToEdgeControllerFactory.createForViewAndObserveSupplier(
                            mView.getRecyclerView(), edgeToEdgeSupplier);
        }
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
        mTabListFaviconProvider.destroy();
    }

    private static boolean enableContainment() {
        return SurfaceColorUpdateUtils.isTabGroupListContainmentEnabled();
    }
}
