// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.graphics.drawable.Drawable;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.tab_ui.R;

import java.util.List;

/**
 * Close action for the {@link TabSelectionEditorMenu}.
 */
public class TabSelectionEditorCloseAction extends TabSelectionEditorAction {
    /**
     * Create an action for closing tabs.
     * @param context for loading resources.
     * @param showMode whether to show an action view.
     * @param buttonType the type of the action view.
     * @param iconPosition the position of the icon in the action view.
     */
    public static TabSelectionEditorAction createAction(Context context, @ShowMode int showMode,
            @ButtonType int buttonType, @IconPosition int iconPosition) {
        Drawable drawable = AppCompatResources.getDrawable(context, R.drawable.ic_close_tabs_24dp);
        return new TabSelectionEditorCloseAction(showMode, buttonType, iconPosition, drawable);
    }

    private TabSelectionEditorCloseAction(@ShowMode int showMode, @ButtonType int buttonType,
            @IconPosition int iconPosition, Drawable drawable) {
        super(R.id.tab_selection_editor_close_menu_item, showMode, buttonType, iconPosition,
                R.plurals.tab_selection_editor_close_tabs,
                R.plurals.accessibility_tab_selection_editor_close_tabs, drawable);
    }

    @Override
    public void onSelectionStateChange(List<Integer> tabIds) {
        int size = editorSupportsActionOnRelatedTabs()
                ? getTabCountIncludingRelatedTabs(getTabModelSelector(), tabIds)
                : tabIds.size();
        setEnabledAndItemCount(!tabIds.isEmpty(), size);
    }

    @Override
    public boolean performAction(List<Tab> tabs) {
        assert !tabs.isEmpty() : "Close action should not be enabled for no tabs.";

        if (tabs.size() == 1) {
            getTabModelSelector().getCurrentModel().closeTab(
                    tabs.get(0), /*animate=*/false, /*uponExit=*/false, /*canUndo=*/true);
        } else {
            getTabModelSelector().getCurrentModel().closeMultipleTabs(tabs, true);
        }
        RecordUserAction.record("TabMultiSelectV2.CloseTabs");
        return true;
    }

    @Override
    public boolean shouldHideEditorAfterAction() {
        return true;
    }
}
