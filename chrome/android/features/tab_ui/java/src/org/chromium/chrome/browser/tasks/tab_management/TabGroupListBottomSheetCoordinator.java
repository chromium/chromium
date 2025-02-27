// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabGroupListBottomSheetProperties.ADD_TO_GROUP_VISIBLE;

import android.content.Context;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingServiceFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabFavicon;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/** Coordinator for the Tab Group List Bottom Sheet. */
@NullMarked
public class TabGroupListBottomSheetCoordinator {
    @IntDef({RowType.EXISTING_GROUP, RowType.NEW_GROUP})
    @Retention(RetentionPolicy.SOURCE)
    public @interface RowType {
        int NEW_GROUP = 0;
        int EXISTING_GROUP = 1;
    }

    interface TabGroupParityBottomSheetCoordinatorDelegate {
        /** Requests to show the bottom sheet content. */
        boolean requestShowContent();

        /** Hides the bottom sheet. */
        void hide(@StateChangeReason int hideReason);

        /** To be run on sheet close. */
        void onSheetClosed();
    }

    private final TabGroupListBottomSheetView mView;
    private final BottomSheetController mBottomSheetController;
    private final SimpleRecyclerViewAdapter mSimpleRecyclerViewAdapter;
    private final TabListFaviconProvider mTabListFaviconProvider;
    private final TabGroupListBottomSheetMediator mMediator;

    /**
     * @param context The {@link Context} to attach the bottom sheet to.
     * @param profile The current user profile.
     * @param tabGroupCreationDialogManager Used to show the tab group creation dialog.
     * @param filter Used to read current tab groups.
     * @param bottomSheetController Used to interact with the bottom sheet.
     */
    public TabGroupListBottomSheetCoordinator(
            Context context,
            Profile profile,
            TabGroupCreationDialogManager tabGroupCreationDialogManager,
            TabGroupModelFilter filter,
            BottomSheetController bottomSheetController) {
        mView = new TabGroupListBottomSheetView(context);
        mBottomSheetController = bottomSheetController;

        MVCListAdapter.ModelList modelList = new MVCListAdapter.ModelList();
        mSimpleRecyclerViewAdapter =
                new SimpleRecyclerViewAdapter(modelList) {
                    @Override
                    public void onViewRecycled(ViewHolder holder) {
                        if (holder.getItemViewType() == RowType.EXISTING_GROUP) {
                            TabGroupRowViewBinder.onViewRecycled((TabGroupRowView) holder.itemView);
                        }
                        super.onViewRecycled(holder);
                    }
                };

        mSimpleRecyclerViewAdapter.registerType(
                RowType.NEW_GROUP,
                new LayoutViewBuilder<>(R.layout.tab_group_new_group_row),
                TabGroupListBottomSheetNewGroupRowViewBinder::bind);
        mSimpleRecyclerViewAdapter.registerType(
                RowType.EXISTING_GROUP,
                new LayoutViewBuilder<>(R.layout.tab_group_row),
                TabGroupRowViewBinder::bind);
        mView.setRecyclerViewAdapter(mSimpleRecyclerViewAdapter);

        mTabListFaviconProvider =
                new TabListFaviconProvider(
                        context,
                        /* isTabStrip= */ false,
                        R.dimen.default_favicon_corner_radius,
                        TabFavicon::getBitmap);
        FaviconResolver faviconResolver =
                TabGroupListFaviconResolverFactory.build(context, profile, mTabListFaviconProvider);
        @Nullable TabGroupSyncService tabGroupSyncService =
                TabGroupSyncServiceFactory.getForProfile(profile);

        CollaborationService collaborationService =
                CollaborationServiceFactory.getForProfile(profile);

        DataSharingService dataSharingService = DataSharingServiceFactory.getForProfile(profile);

        mMediator =
                new TabGroupListBottomSheetMediator(
                        modelList,
                        filter,
                        tabGroupCreationDialogManager,
                        faviconResolver,
                        tabGroupSyncService,
                        dataSharingService,
                        collaborationService,
                        bottomSheetController,
                        createModel(),
                        createDelegate());
        PropertyModelChangeProcessor.create(
                createModel(), mView, TabGroupListBottomSheetViewBinder::bind);
    }

    /** Creates the delegate. */
    @VisibleForTesting
    TabGroupParityBottomSheetCoordinatorDelegate createDelegate() {
        return new TabGroupParityBottomSheetCoordinatorDelegate() {
            @Override
            public boolean requestShowContent() {
                return mBottomSheetController.requestShowContent(mView, /* animate= */ true);
            }

            @Override
            public void hide(@StateChangeReason int hideReason) {
                mBottomSheetController.hideContent(mView, /* animate= */ true, hideReason);
            }

            @Override
            public void onSheetClosed() {
                destroy();
            }
        };
    }

    /**
     * Requests to show the bottom sheet.
     *
     * @param tabs The list of tabs to be added to a group.
     * @param showAddToGroupRow Whether the 'Add To Group' row should be displayed.
     */
    public void showBottomSheet(List<Tab> tabs, boolean showAddToGroupRow) {
        mMediator.setShowAddToGroupRow(showAddToGroupRow);
        mMediator.requestShowContent(tabs);
    }

    /** Permanently cleans up this component. */
    public void destroy() {
        mSimpleRecyclerViewAdapter.destroy();
        mTabListFaviconProvider.destroy();
    }

    private PropertyModel createModel() {
        return new PropertyModel.Builder(TabGroupListBottomSheetProperties.ALL_KEYS)
                .with(ADD_TO_GROUP_VISIBLE, true)
                .build();
    }
}
