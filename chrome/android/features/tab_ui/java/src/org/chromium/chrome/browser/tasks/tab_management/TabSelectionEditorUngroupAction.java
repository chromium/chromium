// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.graphics.drawable.Drawable;

import androidx.appcompat.content.res.AppCompatResources;

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
     * @param context for loading resources.
     * @param showMode whether to show an action view.
     * @param buttonType the type of the action view.
     * @param iconPosition the position of the icon in the action view.
     */
    public static TabSelectionEditorAction createAction(Context context, @ShowMode int showMode,
            @ButtonType int buttonType, @IconPosition int iconPosition) {
        Drawable drawable = AppCompatResources.getDrawable(context, R.drawable.ic_widgets);
        return new TabSelectionEditorUngroupAction(showMode, buttonType, iconPosition, drawable);
    }

    private TabSelectionEditorUngroupAction(@ShowMode int showMode, @ButtonType int buttonType,
            @IconPosition int iconPosition, Drawable drawable) {
        super(R.id.tab_selection_editor_ungroup_menu_item, showMode, buttonType, iconPosition,
                R.plurals.tab_selection_editor_ungroup_tabs,
                R.plurals.accessibility_tab_selection_editor_ungroup_tabs, drawable);
    }

    @Override
    public void onSelectionStateChange(List<Integer> tabIds) {
        assert getTabModelSelector().getTabModelFilterProvider().getCurrentTabModelFilter()
                        instanceof TabGroupModelFilter;
        // assert !editorSupportsActionOnRelatedTabs()
        //     : "Ungrouping is not supported when actions apply to related tabs.";

        setEnabledAndItemCount(!tabIds.isEmpty(), tabIds.size());
    }

    @Override
    public boolean performAction(List<Tab> tabs) {
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
        RecordUserAction.record("TabMultiSelectV2.UngroupTabs");
        RecordUserAction.record("TabGridDialog.RemoveFromGroup.TabMultiSelect");
        return true;
    }

    @Override
    public boolean shouldHideEditorAfterAction() {
        return true;
    }
}
