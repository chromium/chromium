// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.tab_ui.R;

import java.util.List;

/**
 * Ungroup action for the {@link TabSelectionEditorMenu}.
 */
public class TabSelectionEditorUngroupAction extends TabSelectionEditorAction {
    /**
     * Create an action for ungrouping tabs.
     * @param showMode whether to show an action view.
     * @param buttonType the type of the action view.
     * @param iconPosition the position of the icon in the action view.
     */
    public static TabSelectionEditorAction createAction(
            @ShowMode int showMode, @ButtonType int buttonType, @IconPosition int iconPosition) {
        // TODO(ckitagawa): Load drawable and pass to constructor.
        return new TabSelectionEditorUngroupAction(showMode, buttonType, iconPosition);
    }

    private TabSelectionEditorUngroupAction(
            @ShowMode int showMode, @ButtonType int buttonType, @IconPosition int iconPosition) {
        super(R.id.tab_selection_editor_ungroup_menu_item, showMode, buttonType, iconPosition,
                R.string.tab_grid_dialog_selection_mode_remove,
                R.plurals.accessibility_tab_selection_dialog_remove_button, null);
    }

    @Override
    public void onSelectionStateChange(List<Integer> tabIds) {
        assert getTabModelSelector().getTabModelFilterProvider().getCurrentTabModelFilter()
                        instanceof TabGroupModelFilter;
        assert !editorSupportsActionOnRelatedTabs()
            : "Ungrouping is not supported when actions apply to related tabs.";

        setEnabledAndItemCount(!tabIds.isEmpty(), tabIds.size());
    }

    @Override
    public void performAction(List<Tab> tabs) {
        assert getTabModelSelector().getTabModelFilterProvider().getCurrentTabModelFilter()
                        instanceof TabGroupModelFilter;
        assert !editorSupportsActionOnRelatedTabs()
            : "Ungrouping is not supported when actions apply to related tabs.";

        TabGroupModelFilter filter = (TabGroupModelFilter) getTabModelSelector()
                                             .getTabModelFilterProvider()
                                             .getCurrentTabModelFilter();
        for (Tab tab : tabs) {
            filter.moveTabOutOfGroup(tab.getId());
        }
        RecordUserAction.record("TabGridDialog.RemoveFromGroup.TabMultiSelect");
    }

    @Override
    public boolean shouldHideEditorAfterAction() {
        return true;
    }
}
