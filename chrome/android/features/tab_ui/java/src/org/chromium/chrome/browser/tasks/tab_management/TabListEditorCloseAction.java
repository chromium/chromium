// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.graphics.drawable.Drawable;

import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tasks.tab_management.ActionConfirmationManager.ConfirmationResult;
import org.chromium.chrome.browser.tasks.tab_management.TabUiMetricsHelper.TabListEditorActionMetricGroups;
import org.chromium.chrome.tab_ui.R;

import java.util.List;
import java.util.stream.Collectors;

/** Close action for the {@link TabListEditorMenu}. */
public class TabListEditorCloseAction extends TabListEditorAction {
    /**
     * Create an action for closing tabs.
     *
     * @param context for loading resources.
     * @param showMode whether to show an action view.
     * @param buttonType the type of the action view.
     * @param iconPosition the position of the icon in the action view.
     * @param actionConfirmationManager used for showing confirmation dialogs.
     */
    public static TabListEditorAction createAction(
            Context context,
            @ShowMode int showMode,
            @ButtonType int buttonType,
            @IconPosition int iconPosition,
            @Nullable ActionConfirmationManager actionConfirmationManager) {
        Drawable drawable = AppCompatResources.getDrawable(context, R.drawable.ic_close_tabs_24dp);
        return new TabListEditorCloseAction(
                showMode, buttonType, iconPosition, drawable, actionConfirmationManager);
    }

    private @Nullable final ActionConfirmationManager mActionConfirmationManager;

    private TabListEditorCloseAction(
            @ShowMode int showMode,
            @ButtonType int buttonType,
            @IconPosition int iconPosition,
            Drawable drawable,
            @Nullable ActionConfirmationManager actionConfirmationManager) {
        super(
                R.id.tab_list_editor_close_menu_item,
                showMode,
                buttonType,
                iconPosition,
                R.plurals.tab_selection_editor_close_tabs,
                R.plurals.accessibility_tab_selection_editor_close_tabs,
                drawable);
        mActionConfirmationManager = actionConfirmationManager;
    }

    @Override
    public void onSelectionStateChange(List<Integer> tabIds) {
        int size =
                editorSupportsActionOnRelatedTabs()
                        ? getTabCountIncludingRelatedTabs(getTabGroupModelFilter(), tabIds)
                        : tabIds.size();
        setEnabledAndItemCount(!tabIds.isEmpty(), size);
    }

    @Override
    public boolean performAction(List<Tab> tabs) {
        assert !tabs.isEmpty() : "Close action should not be enabled for no tabs.";

        if (getTabGroupModelFilter().isIncognito() || mActionConfirmationManager == null) {
            doRemoveTabs(tabs, /* allowUndo= */ true);
            return true;
        }

        Callback<Integer> onResult =
                (@ConfirmationResult Integer result) -> {
                    if (result != ConfirmationResult.CONFIRMATION_NEGATIVE) {
                        doRemoveTabs(tabs, result == ConfirmationResult.IMMEDIATE_CONTINUE);
                    }
                };

        List<Integer> tabIds = tabs.stream().map(Tab::getId).collect(Collectors.toList());
        mActionConfirmationManager.processCloseTabAttempt(tabIds, onResult);

        return true;
    }

    private void doRemoveTabs(List<Tab> tabs, boolean allowUndo) {
        getTabGroupModelFilter()
                .closeTabs(
                        TabClosureParams.closeTabs(tabs)
                                .allowUndo(allowUndo)
                                .hideTabGroups(editorSupportsActionOnRelatedTabs())
                                .build());
        TabUiMetricsHelper.recordSelectionEditorActionMetrics(
                TabListEditorActionMetricGroups.CLOSE);
    }

    @Override
    public boolean shouldHideEditorAfterAction() {
        return true;
    }
}
