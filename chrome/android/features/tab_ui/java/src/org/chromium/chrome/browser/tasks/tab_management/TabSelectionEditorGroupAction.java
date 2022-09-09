// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.tab_ui.R;

import java.util.List;

/**
 * Group action for the {@link TabSelectionEditorMenu}.
 */
public class TabSelectionEditorGroupAction extends TabSelectionEditorAction {
    /**
     * Create an action for grouping tabs.
     * @param showMode whether to show an action view.
     * @param buttonType the type of the action view.
     * @param iconPosition the position of the icon in the action view.
     */
    public static TabSelectionEditorAction createAction(
            @ShowMode int showMode, @ButtonType int buttonType, @IconPosition int iconPosition) {
        // TODO(ckitagawa): Load drawable and pass to constructor.
        return new TabSelectionEditorGroupAction(showMode, buttonType, iconPosition);
    }

    private TabSelectionEditorGroupAction(
            @ShowMode int showMode, @ButtonType int buttonType, @IconPosition int iconPosition) {
        super(R.id.tab_selection_editor_group_menu_item, showMode, buttonType, iconPosition,
                R.string.tab_selection_editor_group,
                R.plurals.accessibility_tab_selection_editor_group_button, null);
    }

    @Override
    public void onSelectionStateChange(List<Integer> tabIds) {
        assert getTabModelSelector().getTabModelFilterProvider().getCurrentTabModelFilter()
                        instanceof TabGroupModelFilter;

        int size = editorSupportsActionOnRelatedTabs()
                ? getTabCountIncludingRelatedTabs(getTabModelSelector(), tabIds)
                : tabIds.size();
        setEnabledAndItemCount(tabIds.size() > 1, size);
    }

    @Override
    public void performAction(List<Tab> tabs) {
        assert getTabModelSelector().getTabModelFilterProvider().getCurrentTabModelFilter()
                        instanceof TabGroupModelFilter;

        TabGroupModelFilter tabGroupModelFilter = (TabGroupModelFilter) getTabModelSelector()
                                                          .getTabModelFilterProvider()
                                                          .getCurrentTabModelFilter();

        Tab destinationTab = getDestinationTab(tabs, getTabModelSelector().getCurrentModel(),
                tabGroupModelFilter, editorSupportsActionOnRelatedTabs());
        tabGroupModelFilter.mergeListOfTabsToGroup(tabs, destinationTab, false, true);

        RecordUserAction.record("TabMultiSelect.Done");
        RecordUserAction.record("TabGroup.Created.TabMultiSelect");
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
     * @param model the {@link TabModel} containing the tabs.
     * @param filter the {@link TabGroupModelFilter} for managing groups.
     * @param actionOnRelatedTabs whether to attempt to merge to groups.
     * @return the tab to merge to.
     */
    private Tab getDestinationTab(List<Tab> tabs, TabModel model, TabGroupModelFilter filter,
            boolean actionOnRelatedTabs) {
        int greatestTabIndex = TabModel.INVALID_TAB_INDEX;
        int smallestGroupIndex = TabModel.INVALID_TAB_INDEX;
        for (Tab tab : tabs) {
            final int index = TabModelUtils.getTabIndexById(model, tab.getId());
            greatestTabIndex = Math.max(index, greatestTabIndex);
            if (actionOnRelatedTabs && filter.hasOtherRelatedTabs(tab)) {
                smallestGroupIndex = (smallestGroupIndex == TabModel.INVALID_TAB_INDEX)
                        ? index
                        : Math.min(index, smallestGroupIndex);
            }
        }
        return model.getTabAt((smallestGroupIndex != TabModel.INVALID_TAB_INDEX)
                        ? smallestGroupIndex
                        : greatestTabIndex);
    }
}
