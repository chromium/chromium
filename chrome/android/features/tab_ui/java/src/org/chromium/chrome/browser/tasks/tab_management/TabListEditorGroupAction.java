// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.graphics.drawable.Drawable;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabUiMetricsHelper.TabListEditorActionMetricGroups;
import org.chromium.chrome.tab_ui.R;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;

/** Group action for the {@link TabListEditorMenu}. */
public class TabListEditorGroupAction extends TabListEditorAction {
    /**
     * Create an action for grouping tabs.
     * @param context for loading resources.
     * @param showMode whether to show an action view.
     * @param buttonType the type of the action view.
     * @param iconPosition the position of the icon in the action view.
     */
    public static TabListEditorAction createAction(
            Context context,
            @ShowMode int showMode,
            @ButtonType int buttonType,
            @IconPosition int iconPosition) {
        Drawable drawable = AppCompatResources.getDrawable(context, R.drawable.ic_widgets);
        return new TabListEditorGroupAction(showMode, buttonType, iconPosition, drawable);
    }

    private TabListEditorGroupAction(
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
    }

    @Override
    public void onSelectionStateChange(List<Integer> tabIds) {
        int size =
                editorSupportsActionOnRelatedTabs()
                        ? getTabCountIncludingRelatedTabs(getTabGroupModelFilter(), tabIds)
                        : tabIds.size();

        boolean isEnabled = tabIds.size() > 1;
        if (ChromeFeatureList.sAndroidTabGroupStableIds.isEnabled() && tabIds.size() == 1) {
            TabGroupModelFilter filter = getTabGroupModelFilter();
            Tab tab = TabModelUtils.getTabById(filter.getTabModel(), tabIds.get(0));
            isEnabled = tab != null && !filter.isTabInTabGroup(tab);
        }
        setEnabledAndItemCount(isEnabled, size);
    }

    @Override
    public boolean performAction(List<Tab> tabs) {
        TabGroupModelFilter tabGroupModelFilter = getTabGroupModelFilter();

        if (ChromeFeatureList.sAndroidTabGroupStableIds.isEnabled() && tabs.size() == 1) {
            Tab tab = tabs.get(0);
            if (tabGroupModelFilter.isTabInTabGroup(tab)) return true;

            tabGroupModelFilter.createSingleTabGroup(tab, /* notify= */ true);
            return true;
        }

        HashSet<Tab> selectedTabs = new HashSet<>(tabs);
        Tab destinationTab =
                getDestinationTab(
                        tabs,
                        tabGroupModelFilter,
                        editorSupportsActionOnRelatedTabs());
        List<Tab> relatedTabs = tabGroupModelFilter.getRelatedTabList(destinationTab.getId());
        selectedTabs.removeAll(relatedTabs);

        // Sort tabs by index prevent visual bugs when undoing.
        List<Tab> sortedTabs = new ArrayList<>(selectedTabs.size());
        TabModel model = tabGroupModelFilter.getTabModel();
        for (int i = 0; i < model.getCount(); i++) {
            Tab tab = model.getTabAt(i);
            if (!selectedTabs.contains(tab)) continue;

            sortedTabs.add(tab);
        }

        // Use true for "isSameGroup" to avoid updating the title multiple times.
        tabGroupModelFilter.mergeListOfTabsToGroup(
                sortedTabs, destinationTab, /* isSameGroup= */ true, /* notify= */ true);

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
     * group with the smallest group index. Otherwise, all selected items are merge to the tab with
     * the largest tab index.
     * @param tabs the list of all tabs to merge.
     * @param filter the {@link TabGroupModelFilter} for managing groups.
     * @param actionOnRelatedTabs whether to attempt to merge to groups.
     * @return the tab to merge to.
     */
    private Tab getDestinationTab(
            List<Tab> tabs,
            TabGroupModelFilter filter,
            boolean actionOnRelatedTabs) {
        TabModel model = filter.getTabModel();
        int greatestTabIndex = TabModel.INVALID_TAB_INDEX;
        int smallestGroupIndex = TabModel.INVALID_TAB_INDEX;
        for (Tab tab : tabs) {
            final int index = TabModelUtils.getTabIndexById(model, tab.getId());
            greatestTabIndex = Math.max(index, greatestTabIndex);
            if (actionOnRelatedTabs && filter.isTabInTabGroup(tab)) {
                smallestGroupIndex =
                        (smallestGroupIndex == TabModel.INVALID_TAB_INDEX)
                                ? index
                                : Math.min(index, smallestGroupIndex);
            }
        }
        return model.getTabAt(
                (smallestGroupIndex != TabModel.INVALID_TAB_INDEX)
                        ? smallestGroupIndex
                        : greatestTabIndex);
    }
}
