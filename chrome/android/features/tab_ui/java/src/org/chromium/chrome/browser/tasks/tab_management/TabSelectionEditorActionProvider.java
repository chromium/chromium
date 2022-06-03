// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/**
 * Provider of actions for a list of selected tabs in {@link TabSelectionEditorMediator}.
 */
class TabSelectionEditorActionProvider {
    @IntDef({TabSelectionEditorAction.UNDEFINED_ACTION, TabSelectionEditorAction.GROUP,
            TabSelectionEditorAction.UNGROUP, TabSelectionEditorAction.CLOSE})
    @Retention(RetentionPolicy.SOURCE)
    @interface TabSelectionEditorAction {
        int UNDEFINED_ACTION = 0;
        int GROUP = 1;
        int UNGROUP = 2;
        int CLOSE = 3;
        int NUM_ENTRIES = 4;
    }

    private final TabSelectionEditorCoordinator
            .TabSelectionEditorController mTabSelectionEditorController;
    private final @TabSelectionEditorAction int mAction;

    /**
     * Construct {@link TabSelectionEditorActionProvider} with customized process selected tabs
     * action.
     * @see TabSelectionEditorActionProvider#processSelectedTabs(List, TabModelSelector).
     */
    TabSelectionEditorActionProvider() {
        mTabSelectionEditorController = null;
        mAction = TabSelectionEditorAction.UNDEFINED_ACTION;
    }

    /**
     * Construct {@link TabSelectionEditorActionProvider} with defined
     * {@link TabSelectionEditorAction}.
     *
     * @param tabSelectionEditorController Controller that associated with the TabSelectionEditor.
     * @param action {@link TabSelectionEditorAction} to provide.
     */
    TabSelectionEditorActionProvider(
            TabSelectionEditorCoordinator.TabSelectionEditorController tabSelectionEditorController,
            @TabSelectionEditorAction int action) {
        mTabSelectionEditorController = tabSelectionEditorController;
        mAction = action;
    }

    /**
     * Defines how to process {@code selectedTabs} based on the {@link TabSelectionEditorAction}
     * specified in the constructor. If {@link TabSelectionEditorAction} is not specified, the
     * caller must override this method.
     *
     * @param selectedTabs The list of selected tabs to process.
     * @param tabModelSelector {@link TabModelSelector} to use.
     */
    void processSelectedTabs(List<Tab> selectedTabs, TabModelSelector tabModelSelector) {
        assert !(mAction == TabSelectionEditorAction.GROUP)
                        && !(mAction == TabSelectionEditorAction.UNGROUP)
                || tabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter()
                                instanceof TabGroupModelFilter;

        switch (mAction) {
            case TabSelectionEditorAction.GROUP:
                Tab destinationTab = getDestinationTab(selectedTabs, tabModelSelector);

                TabGroupModelFilter tabGroupModelFilter =
                        (TabGroupModelFilter) tabModelSelector.getTabModelFilterProvider()
                                .getCurrentTabModelFilter();
                tabGroupModelFilter.mergeListOfTabsToGroup(
                        selectedTabs, destinationTab, false, true);
                mTabSelectionEditorController.hide();

                RecordUserAction.record("TabMultiSelect.Done");
                RecordUserAction.record("TabGroup.Created.TabMultiSelect");
                break;
            case TabSelectionEditorAction.UNGROUP:
                TabGroupModelFilter filter =
                        (TabGroupModelFilter) tabModelSelector.getTabModelFilterProvider()
                                .getCurrentTabModelFilter();
                for (Tab tab : selectedTabs) {
                    filter.moveTabOutOfGroup(tab.getId());
                }
                mTabSelectionEditorController.hide();
                RecordUserAction.record("TabGridDialog.RemoveFromGroup.TabMultiSelect");
                break;
            case TabSelectionEditorAction.CLOSE:
                tabModelSelector.getCurrentModel().closeMultipleTabs(selectedTabs, true);
                mTabSelectionEditorController.hide();
                break;
            case TabSelectionEditorAction.UNDEFINED_ACTION:
            default:
                assert false : "TabSelectionEditorActionProvider must override"
                               + "processSelectedTab() if mAction is not pre-defined with"
                               + "TabSelectionEditorAction.";
        }
    }

    /**
     * @return The {@link Tab} that has the greatest index in TabModel among the given list of tabs.
     */
    private Tab getDestinationTab(List<Tab> tabs, TabModelSelector tabModelSelector) {
        int greatestIndex = TabModel.INVALID_TAB_INDEX;
        for (int i = 0; i < tabs.size(); i++) {
            int index = TabModelUtils.getTabIndexById(
                    tabModelSelector.getCurrentModel(), tabs.get(i).getId());
            greatestIndex = Math.max(index, greatestIndex);
        }
        return tabModelSelector.getCurrentModel().getTabAt(greatestIndex);
    }
}
