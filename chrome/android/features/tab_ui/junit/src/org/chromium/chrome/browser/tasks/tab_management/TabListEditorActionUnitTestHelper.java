// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.Mockito.when;

import androidx.annotation.Nullable;

import org.chromium.base.Token;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

/** Helper for setting up mock tabs and groups for TabListEditor*ActionUnitTests. */
public class TabListEditorActionUnitTestHelper {
    /** Defines a group of tabs and its selection state. */
    public static class TabIdGroup {
        private final int[] mTabIds;
        private final boolean mIsGroup;
        private final boolean mSelected;
        private final boolean mIsCollaboration;

        /**
         * @param tabIds the tab ids.
         * @param isGroup whether the tab is a group.
         * @param selected whether the group is selected.
         * @param isCollaboration whether the tab group is a collaboration.
         */
        TabIdGroup(int[] tabIds, boolean isGroup, boolean selected, boolean isCollaboration) {
            mTabIds = tabIds;
            mIsGroup = isGroup;
            mSelected = selected;
            mIsCollaboration = isCollaboration;
        }

        boolean isSelected() {
            return mSelected;
        }

        boolean isGroup() {
            return mIsGroup;
        }

        @Nullable
        String getCollaborationId() {
            return mIsCollaboration ? getTabGroupId().toString() + "_collaboration" : null;
        }

        int getRootId() {
            return mTabIds[0];
        }

        @Nullable
        Token getTabGroupId() {
            return mIsGroup ? new Token(1L, mTabIds[0]) : null;
        }

        int[] getTabIds() {
            return mTabIds;
        }

        int getTabIdAt(int i) {
            return mTabIds[i];
        }
    }

    /**
     * Holds lists of selected tabs that may be needed by the test and that will be used by the
     * {@link TabListEditorAction}.
     */
    public static class TabListHolder {
        private List<Tab> mSelectedTabs;
        private List<Tab> mSelectedAndRelatedTabs;

        /**
         * @param selectedTabs the selected tabs in the TabListEditor.
         * @param selectedAndRelatedTabs the selected tabs and their related tabs.
         */
        TabListHolder(List<Tab> selectedTabs, List<Tab> selectedAndRelatedTabs) {
            mSelectedTabs = selectedTabs;
            mSelectedAndRelatedTabs = selectedAndRelatedTabs;
        }

        List<Tab> getSelectedTabs() {
            return mSelectedTabs;
        }

        List<Tab> getSelectedAndRelatedTabs() {
            return mSelectedAndRelatedTabs;
        }

        List<Integer> getSelectedTabIds() {
            List<Integer> tabIds = new ArrayList<>();
            for (Tab tab : mSelectedTabs) {
                tabIds.add(tab.getId());
            }
            return tabIds;
        }
    }

    /**
     * Adds the tabs described tabs to mock objects to set up an Action unit test.
     *
     * @param tabModel a {@link MockTabModel}.
     * @param filter a mocked {@link TabGroupModelFilter}.
     * @param tabGroupSyncService a mocked {@link TabGroupSyncService}.
     * @param selectionDelegate a mocked {@link SelectionDelegate}.
     * @param tabIdGroups defining the tab structure.
     * @param deterministicSetOrder allow arbitrary selection order.
     */
    public static TabListHolder configureTabs(
            MockTabModel tabModel,
            TabGroupModelFilter filter,
            TabGroupSyncService tabGroupSyncService,
            SelectionDelegate<Integer> selectionDelegate,
            List<TabIdGroup> tabIdGroups,
            boolean deterministicSetOrder) {
        List<Tab> selectedTabs = new ArrayList<>();
        List<Tab> selectedAndRelatedTabs = new ArrayList<>();
        Set<Integer> selectedTabIds =
                deterministicSetOrder ? new LinkedHashSet<Integer>() : new HashSet<Integer>();

        for (TabIdGroup group : tabIdGroups) {
            List<Tab> groupTabs = new ArrayList<>();
            List<SavedTabGroupTab> savedTabs = new ArrayList<>();
            for (int tabId : group.getTabIds()) {
                Tab tab = tabModel.addTab(tabId);
                tab.setRootId(group.getRootId());
                tab.setTabGroupId(group.getTabGroupId());
                if (group.isSelected() && groupTabs.isEmpty()) {
                    selectedTabs.add(tab);
                }
                when(filter.isTabInTabGroup(tab)).thenReturn(group.getTabGroupId() != null);
                groupTabs.add(tab);

                SavedTabGroupTab savedTab = new SavedTabGroupTab();
                savedTab.localId = tabId;
                savedTabs.add(savedTab);
            }
            if (group.isSelected()) {
                selectedTabIds.add(group.getTabIdAt(0));
                selectedAndRelatedTabs.addAll(groupTabs);
            }
            groupTabs.get(0).setRootId(group.getTabIdAt(0));
            when(filter.getRelatedTabList(group.getTabIdAt(0))).thenReturn(groupTabs);
            when(filter.getRelatedTabCountForRootId(group.getTabIdAt(0)))
                    .thenReturn(groupTabs.size());

            if (!group.isGroup() || tabGroupSyncService == null) continue;

            LocalTabGroupId localTabGroupId = new LocalTabGroupId(group.getTabGroupId());
            SavedTabGroup savedGroup = new SavedTabGroup();
            savedGroup.localId = localTabGroupId;
            savedGroup.savedTabs = savedTabs;
            savedGroup.collaborationId = group.getCollaborationId();

            when(tabGroupSyncService.getGroup(localTabGroupId)).thenReturn(savedGroup);
        }
        when(selectionDelegate.getSelectedItems()).thenReturn(selectedTabIds);
        return new TabListHolder(selectedTabs, selectedAndRelatedTabs);
    }
}
