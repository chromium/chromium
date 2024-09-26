// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.graphics.drawable.Drawable;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.tab_management.TabUiMetricsHelper.TabListEditorActionMetricGroups;
import org.chromium.chrome.tab_ui.R;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/** Select all and deselect all toggle action for the {@link TabListEditorMenu}. */
public class TabListEditorSelectionAction extends TabListEditorAction {
    private Context mContext;
    private @ActionState int mActionState;
    private final Drawable mSelectAllIcon;
    private final Drawable mDeselectAllIcon;

    @IntDef({ActionState.UNKNOWN, ActionState.SELECT_ALL, ActionState.DESELECT_ALL})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ActionState {
        int UNKNOWN = 0;
        int SELECT_ALL = 1;
        int DESELECT_ALL = 2;
    }

    /**
     * Create an action for closing tabs.
     *
     * @param context to load drawable from.
     * @param showMode whether to show an action view.
     * @param buttonType the type of the action view.
     * @param iconPosition the position of the icon in the action view.
     */
    public static TabListEditorAction createAction(
            Context context,
            @ShowMode int showMode,
            @ButtonType int buttonType,
            @IconPosition int iconPosition) {
        Drawable selectAllIcon =
                AppCompatResources.getDrawable(context, R.drawable.ic_select_all_24dp);
        Drawable deselectAllIcon =
                AppCompatResources.getDrawable(context, R.drawable.ic_deselect_all_24dp);
        return new TabListEditorSelectionAction(
                context, showMode, buttonType, iconPosition, selectAllIcon, deselectAllIcon);
    }

    @VisibleForTesting
    TabListEditorSelectionAction(
            Context context,
            @ShowMode int showMode,
            @ButtonType int buttonType,
            @IconPosition int iconPosition,
            Drawable selectAllIcon,
            Drawable deselectAllIcon) {
        super(
                R.id.tab_list_editor_selection_menu_item,
                showMode,
                buttonType,
                iconPosition,
                R.string.tab_selection_editor_select_all,
                null,
                selectAllIcon);

        mContext = context;
        mActionState = ActionState.UNKNOWN;
        mSelectAllIcon = selectAllIcon;
        mDeselectAllIcon = deselectAllIcon;
        getPropertyModel().set(TabListEditorActionProperties.SHOULD_DISMISS_MENU, false);
        updateState(ActionState.SELECT_ALL);
    }

    @Override
    public boolean shouldNotifyObserversOfAction() {
        return false;
    }

    @Override
    public void onSelectionStateChange(List<Integer> tabIds) {
        setEnabledAndItemCount(true, tabIds.size());
        updateState(
                getActionDelegate().areAllTabsSelected()
                        ? ActionState.DESELECT_ALL
                        : ActionState.SELECT_ALL);
    }

    @Override
    public boolean performAction(List<Tab> tabs) {
        if (mActionState == ActionState.SELECT_ALL) {
            getActionDelegate().selectAll();
            TabUiMetricsHelper.recordSelectionEditorActionMetrics(
                    TabListEditorActionMetricGroups.SELECT_ALL);
        } else if (mActionState == ActionState.DESELECT_ALL) {
            getActionDelegate().deselectAll();
            TabUiMetricsHelper.recordSelectionEditorActionMetrics(
                    TabListEditorActionMetricGroups.DESELECT_ALL);
        } else {
            assert false : "Invalid selection state";
        }
        return true;
    }

    @Override
    public boolean shouldHideEditorAfterAction() {
        return false;
    }

    private void updateState(@ActionState int selectionState) {
        if (mActionState == selectionState) return;

        mActionState = selectionState;

        if (mActionState == ActionState.SELECT_ALL) {
            getPropertyModel()
                    .set(
                            TabListEditorActionProperties.TITLE_RESOURCE_ID,
                            R.string.tab_selection_editor_select_all);
            getPropertyModel().set(TabListEditorActionProperties.ICON, mSelectAllIcon);
        } else if (mActionState == ActionState.DESELECT_ALL) {
            getPropertyModel()
                    .set(
                            TabListEditorActionProperties.TITLE_RESOURCE_ID,
                            R.string.tab_selection_editor_deselect_all);
            getPropertyModel().set(TabListEditorActionProperties.ICON, mDeselectAllIcon);
        } else {
            assert false : "Invalid selection state";
        }
    }
}
