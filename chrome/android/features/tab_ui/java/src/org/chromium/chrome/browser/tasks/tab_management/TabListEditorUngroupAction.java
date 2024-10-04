// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.graphics.drawable.Drawable;

import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.ActionConfirmationManager.ConfirmationResult;
import org.chromium.chrome.browser.tasks.tab_management.TabUiMetricsHelper.TabListEditorActionMetricGroups;
import org.chromium.chrome.tab_ui.R;

import java.util.List;
import java.util.stream.Collectors;

/** Ungroup action for the {@link TabListEditorMenu}. */
public class TabListEditorUngroupAction extends TabListEditorAction {
    /**
     * Create an action for ungrouping tabs.
     *
     * @param context for loading resources.
     * @param showMode whether to show an action view.
     * @param buttonType the type of the action view.
     * @param iconPosition the position of the icon in the action view.
     * @param actionConfirmationManager To show confirmation dialogs.
     */
    public static TabListEditorAction createAction(
            Context context,
            @ShowMode int showMode,
            @ButtonType int buttonType,
            @IconPosition int iconPosition,
            @Nullable ActionConfirmationManager actionConfirmationManager) {
        Drawable drawable = AppCompatResources.getDrawable(context, R.drawable.ic_widgets);
        return new TabListEditorUngroupAction(
                showMode, buttonType, iconPosition, drawable, actionConfirmationManager);
    }

    private final ActionConfirmationManager mActionConfirmationManager;

    private TabListEditorUngroupAction(
            @ShowMode int showMode,
            @ButtonType int buttonType,
            @IconPosition int iconPosition,
            Drawable drawable,
            @Nullable ActionConfirmationManager actionConfirmationManager) {
        super(
                R.id.tab_list_editor_ungroup_menu_item,
                showMode,
                buttonType,
                iconPosition,
                R.plurals.tab_selection_editor_ungroup_tabs,
                R.plurals.accessibility_tab_selection_editor_ungroup_tabs,
                drawable);
        mActionConfirmationManager = actionConfirmationManager;
    }

    @Override
    public void onSelectionStateChange(List<Integer> tabIds) {
        setEnabledAndItemCount(!tabIds.isEmpty(), tabIds.size());
    }

    @Override
    public boolean performAction(List<Tab> tabsToUngroup) {
        assert !editorSupportsActionOnRelatedTabs()
                : "Ungrouping is not supported when actions apply to related tabs.";

        if (tabsToUngroup == null || tabsToUngroup.isEmpty()) return false;

        TabGroupModelFilter filter = getTabGroupModelFilter();
        if (mActionConfirmationManager == null || filter.isIncognito()) {
            doRemoveTabs(tabsToUngroup);
            return true;
        }

        Tab firstTab = tabsToUngroup.get(0);
        List<Tab> relatedTabs = filter.getRelatedTabList(firstTab.getId());
        // Only trigger trigger confirmation when all the tabs are being removed, as that is when
        // the group will be deleted as a result.
        if (relatedTabs.size() <= tabsToUngroup.size()) {
            List<Integer> tabIdList =
                    tabsToUngroup.stream().map(Tab::getId).collect(Collectors.toList());
            mActionConfirmationManager.processUngroupTabAttempt(
                    tabIdList,
                    (@ConfirmationResult Integer result) -> {
                        if (result != ConfirmationResult.CONFIRMATION_NEGATIVE) {
                            doRemoveTabs(tabsToUngroup);
                        }
                    });
        } else {
            doRemoveTabs(tabsToUngroup);
        }

        return true;
    }

    private void doRemoveTabs(List<Tab> tabs) {
        TabGroupModelFilter filter = getTabGroupModelFilter();
        for (Tab tab : tabs) {
            filter.moveTabOutOfGroupInDirection(tab.getId(), /* trailing= */ true);
        }
        TabUiMetricsHelper.recordSelectionEditorActionMetrics(
                TabListEditorActionMetricGroups.UNGROUP);
    }

    @Override
    public boolean shouldHideEditorAfterAction() {
        return true;
    }
}
