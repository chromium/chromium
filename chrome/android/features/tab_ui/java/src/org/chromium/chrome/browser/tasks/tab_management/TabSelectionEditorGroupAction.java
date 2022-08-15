// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
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

        setEnabledAndItemCount(tabIds.size() > 1, tabIds.size());
    }

    @Override
    public void performAction(List<Tab> tabs) {
        assert getTabModelSelector().getTabModelFilterProvider().getCurrentTabModelFilter()
                        instanceof TabGroupModelFilter;

        Tab destinationTab = getDestinationTab(tabs, getTabModelSelector());
        TabGroupModelFilter tabGroupModelFilter = (TabGroupModelFilter) getTabModelSelector()
                                                          .getTabModelFilterProvider()
                                                          .getCurrentTabModelFilter();
        tabGroupModelFilter.mergeListOfTabsToGroup(tabs, destinationTab, false, true);

        RecordUserAction.record("TabMultiSelect.Done");
        RecordUserAction.record("TabGroup.Created.TabMultiSelect");
    }

    @Override
    public boolean shouldHideEditorAfterAction() {
        return true;
    }

    private Tab getDestinationTab(List<Tab> tabs, TabModelSelector tabModelSelector) {
        int greatestIndex = TabModel.INVALID_TAB_INDEX;
        for (int i = 0; i < tabs.size(); i++) {
            final int index = TabModelUtils.getTabIndexById(
                    tabModelSelector.getCurrentModel(), tabs.get(i).getId());
            greatestIndex = Math.max(index, greatestIndex);
        }
        return tabModelSelector.getCurrentModel().getTabAt(greatestIndex);
    }
}
