// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.graphics.drawable.Drawable;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter.MergeNotificationType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabUiMetricsHelper.TabListEditorActionMetricGroups;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.util.motion.MotionEventInfo;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;

/**
 * Legacy group action for the {@link TabListEditorMenu}. This menu item will be replaced with new
 * items allowing for more explicit control of merging and creation of tab groups.
 */
@NullMarked
public class TabListEditorLegacyGroupAction extends TabListEditorAction {
    private final TabGroupCreationDialogManager mTabGroupCreationDialogManager;

    /**
     * Create an action for grouping tabs.
     *
     * @param context for loading resources.
     * @param tabGroupCreationDialogManager the manager for showing a dialog on group creation.
     * @param showMode whether to show an action view.
     * @param buttonType the type of the action view.
     * @param iconPosition the position of the icon in the action view.
     */
    public static TabListEditorAction createAction(
            Context context,
            TabGroupCreationDialogManager tabGroupCreationDialogManager,
            @ShowMode int showMode,
            @ButtonType int buttonType,
            @IconPosition int iconPosition) {
        Drawable drawable = AppCompatResources.getDrawable(context, R.drawable.ic_widgets);
        return new TabListEditorLegacyGroupAction(
                tabGroupCreationDialogManager, showMode, buttonType, iconPosition, drawable);
    }

    private TabListEditorLegacyGroupAction(
            TabGroupCreationDialogManager tabGroupCreationDialogManager,
            @ShowMode int showMode,
            @ButtonType int buttonType,
            @IconPosition int iconPosition,
            Drawable drawable) {
        super(
                R.id.tab_list_editor_group_menu_item,
                showMode,
                buttonType,
                iconPosition,
                R.plurals.tab_selection_editor_group_tabs,
                R.plurals.accessibility_tab_selection_editor_group_tabs,
                drawable);

        mTabGroupCreationDialogManager = tabGroupCreationDialogManager;
    }

    /**
     * Called when the selected set of tabs changes. Each tab or tab group is represented by a
     * single Tab ID. For tabs this is just the tab's ID, for a tab group one of the tabs in the
     * group has its Tab ID used.
     */
    @Override
    public void onSelectionStateChange(List<TabListEditorItemSelectionId> itemIds) {
        int tabCount =
                editorSupportsActionOnRelatedTabs()
                        ? getTabCountIncludingRelatedTabs(getTabGroupModelFilter(), itemIds)
                        : itemIds.size();

        TabGroupModelFilter filter = getTabGroupModelFilter();
        TabModel tabModel = filter.getTabModel();

        boolean isEnabled = true;
        int tabIdsSize = itemIds.size();
        if (tabIdsSize == 0) {
            isEnabled = false;
        } else if (tabIdsSize == 1) {
            assert !itemIds.get(0).isTabGroupSyncId();
            if (itemIds.get(0).isTabId()) {
                Tab tab = tabModel.getTabById(itemIds.get(0).getTabId());
                isEnabled = tab != null && !filter.isTabInTabGroup(tab);
            }
        } else {
            isEnabled = !hasMultipleCollaborations(tabModel, itemIds);
        }
        setEnabledAndItemCount(isEnabled, tabCount);
    }

    @Override
    public boolean performAction(
            List<Tab> tabs,
            List<String> tabGroupSyncIds,
            @Nullable MotionEventInfo triggeringMotion) {
        TabGroupModelFilter tabGroupModelFilter = getTabGroupModelFilter();

        if (tabs.size() == 1) {
            Tab tab = tabs.get(0);
            if (tabGroupModelFilter.isTabInTabGroup(tab)) return true;

            tabGroupModelFilter.createSingleTabGroup(tab);
            mTabGroupCreationDialogManager.showDialog(tab.getTabGroupId(), tabGroupModelFilter);
            return true;
        }

        HashSet<Tab> selectedTabs = new HashSet<>(tabs);
        Tab destinationTab =
                getDestinationTab(tabs, tabGroupModelFilter, editorSupportsActionOnRelatedTabs());
        List<Tab> relatedTabs = tabGroupModelFilter.getRelatedTabList(destinationTab.getId());
        selectedTabs.removeAll(relatedTabs);

        // Sort tabs by index prevent visual bugs when undoing.
        List<Tab> sortedTabs = new ArrayList<>(selectedTabs.size());
        TabModel model = tabGroupModelFilter.getTabModel();
        for (Tab tab : model) {
            if (!selectedTabs.contains(tab)) continue;

            sortedTabs.add(tab);
        }

        boolean willMergingCreateNewGroup = tabGroupModelFilter.willMergingCreateNewGroup(tabs);
        tabGroupModelFilter.mergeListOfTabsToGroup(
                sortedTabs,
                destinationTab,
                /* notify= */ MergeNotificationType.NOTIFY_IF_NOT_NEW_GROUP);

        if (willMergingCreateNewGroup) {
            mTabGroupCreationDialogManager.showDialog(
                    destinationTab.getTabGroupId(), tabGroupModelFilter);
        }

        TabUiMetricsHelper.recordSelectionEditorActionMetrics(
                TabListEditorActionMetricGroups.GROUP);
        return true;
    }

    @Override
    public boolean shouldHideEditorAfterAction() {
        return true;
    }

    /**
     * Finds the tab to merge to. If at least one group is selected, merge all selected items to the
     * group with the smallest group index or the collaboration if only one collaboration is
     * selected. Otherwise, all selected items are merge to the tab with the largest tab index.
     *
     * @param tabs the list of all tabs to merge, this includes all tabs for tab groups.
     * @param filter the {@link TabGroupModelFilter} for managing groups.
     * @param actionOnRelatedTabs whether to attempt to merge to groups.
     * @return the tab to merge to.
     */
    private Tab getDestinationTab(
            List<Tab> tabs, TabGroupModelFilter filter, boolean actionOnRelatedTabs) {
        TabModel model = filter.getTabModel();

        Profile profile = assumeNonNull(model.getProfile());
        @Nullable TabGroupSyncService tabGroupSyncService =
                model.isIncognitoBranded()
                        ? null
                        : TabGroupSyncServiceFactory.getForProfile(profile);

        @Nullable Token collaborationTabGroupId = null;
        int greatestTabIndex = TabModel.INVALID_TAB_INDEX;
        int groupIndex = TabModel.INVALID_TAB_INDEX;
        for (Tab tab : tabs) {
            final int index = TabModelUtils.getTabIndexById(model, tab.getId());
            greatestTabIndex = Math.max(index, greatestTabIndex);
            if (actionOnRelatedTabs && filter.isTabInTabGroup(tab)) {
                if (TabShareUtils.isCollaborationIdValid(
                        TabShareUtils.getCollaborationIdOrNull(
                                tab.getId(), model, tabGroupSyncService))) {
                    if (collaborationTabGroupId == null) {
                        groupIndex = index;
                        collaborationTabGroupId = tab.getTabGroupId();
                    } else if (collaborationTabGroupId.equals(tab.getTabGroupId())) {
                        groupIndex = Math.min(index, groupIndex);
                    } else {
                        assert false : "Merging multiple collaborations is not allowed.";
                    }
                } else if (groupIndex == TabModel.INVALID_TAB_INDEX) {
                    groupIndex = index;
                } else if (collaborationTabGroupId == null) {
                    groupIndex = Math.min(index, groupIndex);
                }
            }
        }

        Tab tab =
                model.getTabAt(
                        (groupIndex != TabModel.INVALID_TAB_INDEX) ? groupIndex : greatestTabIndex);
        return assumeNonNull(tab);
    }

    /**
     * Computes whether multiple collaborations are selected.
     *
     * @param tabModel The {@link TabModel} to use for checking.
     * @param itemIds The list of Tab IDs to check for collaboration membership. For tab groups only
     *     a single tab ID for one of the members of the tab group is provided. This can also be a a
     *     sync ID representing a saved tab group.
     */
    private boolean hasMultipleCollaborations(
            TabModel tabModel, List<TabListEditorItemSelectionId> itemIds) {
        if (tabModel.isIncognitoBranded()) return false;

        Profile profile = assumeNonNull(tabModel.getProfile());
        @Nullable TabGroupSyncService tabGroupSyncService =
                TabGroupSyncServiceFactory.getForProfile(profile);
        if (tabGroupSyncService == null) return false;

        boolean foundCollaboration = false;
        for (TabListEditorItemSelectionId itemId : itemIds) {
            assert !itemId.isTabGroupSyncId();
            if (itemId.isTabId()) {
                if (TabShareUtils.isCollaborationIdValid(
                        TabShareUtils.getCollaborationIdOrNull(
                                itemId.getTabId(), tabModel, tabGroupSyncService))) {
                    if (foundCollaboration) return true;

                    foundCollaboration = true;
                }
            }
        }
        return false;
    }
}
