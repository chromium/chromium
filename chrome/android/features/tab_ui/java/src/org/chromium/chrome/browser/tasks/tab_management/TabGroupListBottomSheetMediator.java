// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tabmodel.TabGroupUtils.findSingleTabGroupIfPresent;

import org.chromium.base.Token;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupListBottomSheetCoordinator.RowType;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupListBottomSheetCoordinator.TabGroupCreationCallback;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupListBottomSheetCoordinator.TabGroupListBottomSheetCoordinatorDelegate;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupListBottomSheetCoordinator.TabMovedCallback;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.HashSet;
import java.util.List;
import java.util.Objects;
import java.util.Set;

/**
 * Mediator class for the Tab Group List Bottom Sheet. This mediator contains the logic for bottom
 * sheet user interactions.
 */
@NullMarked
public class TabGroupListBottomSheetMediator {
    /** Represents a operation to merge tabs to a destination. */
    interface TabGroupListBottomSheetRowMergeOperation {
        /**
         * Merges tabs to a destination.
         *
         * @param tabs The tabs to be added to a tab group.
         * @param destTabId The tab id of the destination tab group.
         * @param tabGroupModelFilter Used to read current tab groups.
         * @param tabMovedCallback Used to follow up on a tab being moved groups or ungrouped.
         */
        default void mergeTabsToDest(
                List<Tab> tabs,
                @TabId int destTabId,
                TabGroupModelFilter tabGroupModelFilter,
                @Nullable TabMovedCallback tabMovedCallback) {
            Tab destTab = tabGroupModelFilter.getTabModel().getTabById(destTabId);
            if (destTab == null) {
                return;
            }

            // TODO(crbug.com/413724490): Replace the ungroup operation with a {@link
            // TabGroupModelFilter}-based solution.
            //
            // Right now, it is only possible to move tabs in 2 ways:
            // - Moving 1 tab within a group to another via the ctx menu 'Move to group' option
            // - Selecting a whole group via the Tab List Editor
            // In the first case, we want to ungroup, since the tab group card needs to be updated.
            // In the second case, we do not want to since the card disappears. This means the
            // 'undo' option will fully be able to undo this action.
            if (tabs.size() == 1) {
                tabGroupModelFilter
                        .getTabUngrouper()
                        .ungroupTabs(tabs, /* trailing= */ false, /* allowDialog= */ false);
            }

            tabGroupModelFilter.mergeListOfTabsToGroup(tabs, destTab, true);
            if (tabMovedCallback != null) {
                tabMovedCallback.onTabMoved();
            }
        }
    }

    private final BottomSheetController mBottomSheetController;
    private final TabGroupListBottomSheetCoordinatorDelegate mDelegate;
    private final ModelList mModelList;
    private final TabGroupModelFilter mFilter;
    private final @Nullable TabMovedCallback mTabMovedCallback;
    private final TabGroupCreationCallback mTabGroupCreationCallback;
    private final FaviconResolver mFaviconResolver;
    private final @Nullable TabGroupSyncService mTabGroupSyncService;
    private final boolean mShowNewGroup;

    private final BottomSheetObserver mBottomSheetObserver =
            new EmptyBottomSheetObserver() {
                @Override
                public void onSheetClosed(@StateChangeReason int reason) {
                    mBottomSheetController.removeObserver(mBottomSheetObserver);
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
     * @param tabMovedCallback Used to follow up on a tab being moved groups or ungrouped.
     * @param faviconResolver Used to fetch favicon images for some tabs.
     * @param tabGroupSyncService Used to fetch synced copy of tab groups.
     * @param bottomSheetController Used to interact with the bottom sheet.
     * @param delegate Called on {@link BottomSheetObserver} calls.
     * @param supportsShowNewGroup Whether the 'New Tab Group' row is supported.
     */
    public TabGroupListBottomSheetMediator(
            ModelList modelList,
            TabGroupModelFilter filter,
            TabGroupCreationCallback tabGroupCreationCallback,
            @Nullable TabMovedCallback tabMovedCallback,
            FaviconResolver faviconResolver,
            @Nullable TabGroupSyncService tabGroupSyncService,
            BottomSheetController bottomSheetController,
            TabGroupListBottomSheetCoordinatorDelegate delegate,
            boolean supportsShowNewGroup) {
        mModelList = modelList;
        mFilter = filter;
        mTabGroupCreationCallback = tabGroupCreationCallback;
        mTabMovedCallback = tabMovedCallback;
        mFaviconResolver = faviconResolver;
        mTabGroupSyncService = tabGroupSyncService;
        mBottomSheetController = bottomSheetController;
        mDelegate = delegate;
        mShowNewGroup = supportsShowNewGroup;
    }

    /**
     * Requests to show the bottom sheet content. Will not show if the user has already accepted the
     * notice.
     *
     * @param tabs The tabs to be added to a tab group.
     */
    void requestShowContent(List<Tab> tabs) {
        // Populate the list of tabs before sending the show-content request to the delegate.
        // This allows us to know the height of the bottom sheet.
        populateList(tabs);
        if (!mDelegate.requestShowContent()) return; // Return early if content didn't actually show
        mBottomSheetController.addObserver(mBottomSheetObserver);
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
        @Nullable Token groupToNotBeIncluded = findSingleTabGroupIfPresent(tabs);
        if (shouldShowNewGroupRow(tabs, groupToNotBeIncluded)) {
            insertNewGroupRow(tabs);
        }

        if (mTabGroupSyncService != null) {
            populateRegularTabGroups(tabs, groupToNotBeIncluded);
        } else {
            populateIncognitoTabGroups(tabs, groupToNotBeIncluded);
        }
    }

    private void populateIncognitoTabGroups(List<Tab> tabs, @Nullable Token groupToNotBeIncluded) {
        for (Token groupId : mFilter.getAllTabGroupIds()) {
            if (Objects.equals(groupToNotBeIncluded, groupId)) {
                continue;
            }

            LocalTabGroupListBottomSheetRowMediator rowMediator =
                    new LocalTabGroupListBottomSheetRowMediator(
                            groupId,
                            mFilter,
                            mFaviconResolver,
                            () -> hide(StateChangeReason.INTERACTION_COMPLETE),
                            mTabMovedCallback,
                            tabs);
            mModelList.add(
                    new MVCListAdapter.ListItem(RowType.EXISTING_GROUP, rowMediator.getModel()));
        }
    }

    private void populateRegularTabGroups(List<Tab> tabs, @Nullable Token groupToFilter) {
        GroupWindowChecker windowChecker = new GroupWindowChecker(mTabGroupSyncService, mFilter);
        List<SavedTabGroup> sortedTabGroups =
                windowChecker.getSortedGroupList(
                        this::shouldShowGroupByState,
                        (a, b) -> Long.compare(b.updateTimeMs, a.updateTimeMs));

        for (SavedTabGroup tabGroup : sortedTabGroups) {
            if (tabGroup.localId != null
                    && Objects.equals(groupToFilter, tabGroup.localId.tabGroupId)) {
                continue;
            }

            TabGroupListBottomSheetRowMediator rowMediator =
                    new TabGroupListBottomSheetRowMediator(
                            tabGroup,
                            mFilter,
                            mFaviconResolver,
                            mTabGroupSyncService,
                            () -> hide(StateChangeReason.INTERACTION_COMPLETE),
                            mTabMovedCallback,
                            tabs);
            mModelList.add(
                    new MVCListAdapter.ListItem(RowType.EXISTING_GROUP, rowMediator.getModel()));
        }
    }

    private void insertNewGroupRow(List<Tab> tabs) {
        Runnable onClickRunnable = () -> createNewGroupForTabs(tabs);

        PropertyModel.Builder builder = new PropertyModel.Builder(TabGroupRowProperties.ALL_KEYS);
        builder.with(TabGroupRowProperties.ROW_CLICK_RUNNABLE, onClickRunnable);
        PropertyModel propertyModel = builder.build();
        mModelList.add(new MVCListAdapter.ListItem(RowType.NEW_GROUP, propertyModel));
    }

    private void createNewGroupForTabs(List<Tab> tabs) {
        RecordUserAction.record("TabGroupParity.BottomSheetRowSelection.NewGroup");
        assert !tabs.isEmpty();
        Tab tab = tabs.get(0);

        if (tabs.size() == 1 && tab.getTabGroupId() == null) {
            mFilter.createSingleTabGroup(tab);
        } else if (tabs.size() == 1 && tab.getTabGroupId() != null) {
            mFilter.getTabUngrouper()
                    .ungroupTabs(tabs, /* trailing= */ false, /* allowDialog= */ false);
            if (mTabMovedCallback != null) {
                mTabMovedCallback.onTabMoved();
            }
            mFilter.createSingleTabGroup(tab);
        } else {
            mFilter.mergeListOfTabsToGroup(tabs, tab, /* notify= */ true);
        }

        hide(StateChangeReason.INTERACTION_COMPLETE);
        Token tabGroupId = tab.getTabGroupId();
        if (tabGroupId == null) return;
        mTabGroupCreationCallback.onTabGroupCreated(tabGroupId);
    }

    /**
     * Whether to show the new group row.
     *
     * <p>Returns true if {@code mShowNewGroup} is true and if:
     *
     * <ul>
     *   <li>None of the tabs are grouped.
     *   <li>There is a single tab to be moved and it is not already in a group or said tab is being
     *       filtered.
     *   <li>The tabs are members of multiple groups.
     * </ul>
     *
     * @param tabs The tabs to be added to a tab group.
     * @param groupToNotBeIncluded The group to not be included in the final tab group list.
     */
    private boolean shouldShowNewGroupRow(List<Tab> tabs, @Nullable Token groupToNotBeIncluded) {
        Set<Token> groupIds = new HashSet<>();
        for (Tab tab : tabs) {
            if (tab.getTabGroupId() != null) {
                groupIds.add(tab.getTabGroupId());
            }
        }

        int numGroups = groupIds.size();
        boolean isSingleTabToBeMoved = tabs.size() == 1;
        boolean singleGroupPredicate =
                numGroups == 1 && (isSingleTabToBeMoved || groupToNotBeIncluded != null);

        return (numGroups == 0 || singleGroupPredicate || numGroups > 1) && mShowNewGroup;
    }

    private boolean shouldShowGroupByState(@GroupWindowState int groupWindowState) {
        return groupWindowState != GroupWindowState.IN_ANOTHER
                && groupWindowState != GroupWindowState.HIDDEN;
    }
}
