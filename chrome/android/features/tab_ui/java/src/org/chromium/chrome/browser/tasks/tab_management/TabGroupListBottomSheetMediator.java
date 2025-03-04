// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupListBottomSheetCoordinator.RowType;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupListBottomSheetCoordinator.TabGroupCreationCallback;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupListBottomSheetCoordinator.TabGroupParityBottomSheetCoordinatorDelegate;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * Mediator class for the Tab Group List Bottom Sheet. This mediator contains the logic for bottom
 * sheet user interactions.
 */
@NullMarked
public class TabGroupListBottomSheetMediator {
    private final BottomSheetController mBottomSheetController;
    private final TabGroupParityBottomSheetCoordinatorDelegate mDelegate;
    private final ModelList mModelList;
    private final TabGroupModelFilter mFilter;
    private final TabGroupCreationCallback mTabGroupCreationCallback;
    private final FaviconResolver mFaviconResolver;
    private final @Nullable TabGroupSyncService mTabGroupSyncService;
    private final DataSharingService mDataSharingService;
    private final CollaborationService mCollaborationService;
    private final boolean mShowNewGroup;

    private final BottomSheetObserver mBottomSheetObserver =
            new EmptyBottomSheetObserver() {
                @Override
                public void onSheetClosed(@StateChangeReason int reason) {
                    mBottomSheetController.removeObserver(mBottomSheetObserver);
                    mDelegate.onSheetClosed();
                    mModelList.clear();
                }

                @Override
                public void onSheetStateChanged(
                        @SheetState int newState, @StateChangeReason int reason) {
                    if (newState != SheetState.HIDDEN) return;
                    onSheetClosed(reason);
                }
            };

    /**
     * @param modelList Side effect is adding items to this list.
     * @param filter Used to read current tab groups.
     * @param tabGroupCreationCallback Used to follow up on tab group creation.
     * @param faviconResolver Used to fetch favicon images for some tabs.
     * @param tabGroupSyncService Used to fetch synced copy of tab groups.
     * @param dataSharingService Used to fetch shared group data.
     * @param collaborationService Used to fetch collaboration group data.
     * @param bottomSheetController Used to interact with the bottom sheet.
     * @param delegate Called on {@link BottomSheetObserver} calls.
     * @param showNewGroupRow Whether the 'New Tab Group' row should be displayed.
     */
    public TabGroupListBottomSheetMediator(
            ModelList modelList,
            TabGroupModelFilter filter,
            TabGroupCreationCallback tabGroupCreationCallback,
            FaviconResolver faviconResolver,
            @Nullable TabGroupSyncService tabGroupSyncService,
            DataSharingService dataSharingService,
            CollaborationService collaborationService,
            BottomSheetController bottomSheetController,
            TabGroupParityBottomSheetCoordinatorDelegate delegate,
            boolean showNewGroupRow) {
        mModelList = modelList;
        mFilter = filter;
        mTabGroupCreationCallback = tabGroupCreationCallback;
        mFaviconResolver = faviconResolver;
        mTabGroupSyncService = tabGroupSyncService;
        mDataSharingService = dataSharingService;
        mCollaborationService = collaborationService;
        mBottomSheetController = bottomSheetController;
        mDelegate = delegate;
        mShowNewGroup = showNewGroupRow;
    }

    /**
     * Requests to show the bottom sheet content. Will not show if the user has already accepted the
     * notice.
     *
     * @param tabs The tabs to be added to a tab group.
     */
    void requestShowContent(List<Tab> tabs) {
        if (!mDelegate.requestShowContent()) return;
        mBottomSheetController.addObserver(mBottomSheetObserver);
        populateList(tabs);
    }

    /** Hides the bottom sheet. */
    void hide(@StateChangeReason int hideReason) {
        mDelegate.hide(hideReason);
    }

    /**
     * Populates the model list for the tab group list bottom sheet.
     *
     * @param tabs The tabs to be added to a tab group.
     */
    private void populateList(List<Tab> tabs) {
        mModelList.clear();
        if (mShowNewGroup) {
            insertAddGroupRow(tabs);
        }

        GroupWindowChecker windowChecker = new GroupWindowChecker(mTabGroupSyncService, mFilter);
        List<SavedTabGroup> sortedTabGroups =
                windowChecker.getSortedGroupList(
                        (a, b) -> Long.compare(b.updateTimeMs, a.updateTimeMs));
        for (SavedTabGroup savedTabGroup : sortedTabGroups) {
            TabGroupListBottomSheetRowMediator rowMediator =
                    new TabGroupListBottomSheetRowMediator(
                            savedTabGroup,
                            mFilter,
                            mTabGroupSyncService,
                            mDataSharingService,
                            mCollaborationService,
                            mFaviconResolver,
                            () -> hide(StateChangeReason.INTERACTION_COMPLETE),
                            tabs);
            mModelList.add(
                    new MVCListAdapter.ListItem(RowType.EXISTING_GROUP, rowMediator.getModel()));
        }
    }

    private void insertAddGroupRow(List<Tab> tabs) {
        Runnable onClickRunnable = () -> createNewGroupForTabs(tabs);

        PropertyModel.Builder builder = new PropertyModel.Builder(TabGroupRowProperties.ALL_KEYS);
        builder.with(TabGroupRowProperties.ROW_CLICK_RUNNABLE, onClickRunnable);
        PropertyModel propertyModel = builder.build();
        mModelList.add(new MVCListAdapter.ListItem(RowType.NEW_GROUP, propertyModel));
    }

    private void createNewGroupForTabs(List<Tab> tabs) {
        assert !tabs.isEmpty();
        Tab tab = tabs.get(0);

        mFilter.mergeListOfTabsToGroup(tabs, tab, true);
        hide(StateChangeReason.INTERACTION_COMPLETE);
        var tabGroupId = tab.getTabGroupId();
        if (tabGroupId == null) return;
        mTabGroupCreationCallback.onTabGroupCreated(tabGroupId);
    }
}
