// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.Mockito.when;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

/**
 * Helper for setting up mock tabs and groups for TabSelectionEditor*ActionUnitTests.
 */
public class TabSelectionEditorActionUnitTestHelper {
    /**
     * Defines a group of tabs and its selection state.
     */
    public static class TabIdGroup {
        private int[] mTabIds;
        private boolean mSelected;

        /**
         * @param tabIds the tab ids.
         * @param selected whether the first tab in the group is selected.
         */
        TabIdGroup(int[] tabIds, boolean selected) {
            mTabIds = tabIds;
            mSelected = selected;
        }

        boolean isSelected() {
            return mSelected;
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
     * {@link TabSelectionEditorAction}.
     */
    public static class TabListHolder {
        private List<Tab> mSelectedTabs;
        private List<Tab> mSelectedAndRelatedTabs;

        /**
         * @param selectedTabs the selected tabs in the TabSelectionEditor.
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
     * @param tabModel a {@link MockTabModel}.
     * @param filter a mocked {@link TabGroupModelFilter}.
     * @param selectionDelegate a mocked {@link SelectionDelegate}.
     * @param tabIdGroups defining the tab structure.
     * @param deterministicSetOrder allow arbitrary selection order.
     */
    public static TabListHolder configureTabs(MockTabModel tabModel, TabGroupModelFilter filter,
            SelectionDelegate<Integer> selectionDelegate, List<TabIdGroup> tabIdGroups,
            boolean deterministicSetOrder) {
        List<Tab> selectedTabs = new ArrayList<>();
        List<Tab> selectedAndRelatedTabs = new ArrayList<>();
        Set<Integer> selectedTabIds =
                deterministicSetOrder ? new LinkedHashSet<Integer>() : new HashSet<Integer>();
        for (TabIdGroup group : tabIdGroups) {
            List<Tab> groupTabs = new ArrayList<Tab>();
            for (int tabId : group.getTabIds()) {
                Tab tab = tabModel.addTab(tabId);
                if (group.isSelected() && groupTabs.isEmpty()) {
                    selectedTabs.add(tab);
                }
                groupTabs.add(tab);
                final boolean hasOtherRelatedTabs = group.getTabIds().length > 1;
                when(filter.hasOtherRelatedTabs(tab)).thenReturn(hasOtherRelatedTabs);
            }
            if (group.isSelected()) {
                selectedTabIds.add(group.getTabIdAt(0));
                selectedAndRelatedTabs.addAll(groupTabs);
            }
            when(filter.getRootId(groupTabs.get(0))).thenReturn(group.getTabIdAt(0));
            when(filter.getRelatedTabList(group.getTabIdAt(0))).thenReturn(groupTabs);
            when(filter.getRelatedTabCountForRootId(group.getTabIdAt(0)))
                    .thenReturn(groupTabs.size());
        }
        when(selectionDelegate.getSelectedItems()).thenReturn(selectedTabIds);
        return new TabListHolder(selectedTabs, selectedAndRelatedTabs);
    }
}
