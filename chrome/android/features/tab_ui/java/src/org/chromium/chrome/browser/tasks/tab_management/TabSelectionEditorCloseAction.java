// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.tab_ui.R;

import java.util.List;

/**
 * Close action for the {@link TabSelectionEditorMenu}.
 */
public class TabSelectionEditorCloseAction extends TabSelectionEditorAction {
    /**
     * Create an action for closing tabs.
     * @param showMode whether to show an action view.
     * @param buttonType the type of the action view.
     * @param iconPosition the position of the icon in the action view.
     */
    public static TabSelectionEditorAction createAction(
            @ShowMode int showMode, @ButtonType int buttonType, @IconPosition int iconPosition) {
        // TODO(ckitagawa): Load drawable and pass to constructor.
        return new TabSelectionEditorCloseAction(showMode, buttonType, iconPosition);
    }

    private TabSelectionEditorCloseAction(
            @ShowMode int showMode, @ButtonType int buttonType, @IconPosition int iconPosition) {
        super(R.id.tab_selection_editor_close_menu_item, showMode, buttonType, iconPosition,
                R.string.tab_suggestion_close_tab_action_button,
                R.plurals.accessibility_tab_suggestion_close_tab_action_button, null);
    }

    @Override
    public void onSelectionStateChange(List<Integer> tabIds) {
        setEnabledAndItemCount(!tabIds.isEmpty(), tabIds.size());
    }

    @Override
    public void performAction(List<Tab> tabs) {
        assert !tabs.isEmpty() : "Close action should not be enabled for no tabs.";

        getTabModelSelector().getCurrentModel().closeMultipleTabs(tabs, true);
    }

    @Override
    public boolean shouldHideEditorAfterAction() {
        return true;
    }
}
