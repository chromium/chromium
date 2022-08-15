// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.InsetDrawable;
import android.graphics.drawable.LayerDrawable;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;
import androidx.core.content.res.ResourcesCompat;
import androidx.vectordrawable.graphics.drawable.AnimatedVectorDrawableCompat;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.tab_ui.R;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/**
 * Select all and deselect all toggle action for the {@link TabSelectionEditorMenu}.
 */
public class TabSelectionEditorSelectionAction extends TabSelectionEditorAction {
    private static final int BACKGROUND = 0;
    private static final int CHECKMARK = 1;

    private Context mContext;
    private @ActionState int mActionState;

    @IntDef({ActionState.UNKNOWN, ActionState.SELECT_ALL, ActionState.DESELECT_ALL})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ActionState {
        int UNKNOWN = 0;
        int SELECT_ALL = 1;
        int DESELECT_ALL = 2;
    }

    /**
     * Create an action for closing tabs.
     * @param context to load drawable from.
     * @param showMode whether to show an action view.
     * @param buttonType the type of the action view.
     * @param iconPosition the position of the icon in the action view.
     * @param isIncognito whether the current tab model is incognito this will update dynamically.
     */
    public static TabSelectionEditorAction createAction(Context context, @ShowMode int showMode,
            @ButtonType int buttonType, @IconPosition int iconPosition, boolean isIncognito) {
        return new TabSelectionEditorSelectionAction(
                context, showMode, buttonType, iconPosition, isIncognito, buildDrawable(context));
    }

    @VisibleForTesting
    TabSelectionEditorSelectionAction(Context context, @ShowMode int showMode,
            @ButtonType int buttonType, @IconPosition int iconPosition, boolean isIncognito,
            Drawable drawable) {
        super(R.id.tab_selection_editor_selection_menu_item, showMode, buttonType, iconPosition,
                R.string.tab_selection_editor_select_all, null, drawable);

        mContext = context;
        mActionState = ActionState.UNKNOWN;
        getPropertyModel().set(TabSelectionEditorActionProperties.ICON_TINT, null);
        getPropertyModel().set(TabSelectionEditorActionProperties.SKIP_ICON_TINT, true);
        updateState(ActionState.SELECT_ALL, isIncognito);
    }

    @Override
    public boolean shouldNotifyObserversOfAction() {
        return false;
    }

    @Override
    public void onSelectionStateChange(List<Integer> tabIds) {
        setEnabledAndItemCount(true, tabIds.size());
        updateState(getActionDelegate().areAllTabsSelected() ? ActionState.DESELECT_ALL
                                                             : ActionState.SELECT_ALL,
                getTabModelSelector().getCurrentModel().isIncognito());
    }

    @Override
    public void performAction(List<Tab> tabs) {
        if (mActionState == ActionState.SELECT_ALL) {
            getActionDelegate().selectAll();
        } else if (mActionState == ActionState.DESELECT_ALL) {
            getActionDelegate().deselectAll();
        } else {
            assert false : "Invalid selection state";
        }
    }

    @Override
    public boolean shouldHideEditorAfterAction() {
        return false;
    }

    private void updateState(@ActionState int selectionState, boolean isIncognito) {
        if (mActionState == selectionState) return;

        mActionState = selectionState;
        final boolean allTabsSelected = mActionState == ActionState.DESELECT_ALL;
        LayerDrawable layers =
                (LayerDrawable) getPropertyModel().get(TabSelectionEditorActionProperties.ICON);

        if (mActionState == ActionState.SELECT_ALL) {
            getPropertyModel().set(TabSelectionEditorActionProperties.TITLE_RESOURCE_ID,
                    R.string.tab_selection_editor_select_all);
            layers.getDrawable(BACKGROUND)
                    .setLevel(
                            mContext.getResources().getInteger(R.integer.list_item_level_default));
            layers.getDrawable(BACKGROUND)
                    .setTint(TabUiThemeProvider.getSelectionActionIconBackgroundColor(
                            mContext, isIncognito, allTabsSelected));

            layers.getDrawable(CHECKMARK).setAlpha(0);
            layers.getDrawable(CHECKMARK).setTint(Color.TRANSPARENT);
        } else if (mActionState == ActionState.DESELECT_ALL) {
            getPropertyModel().set(TabSelectionEditorActionProperties.TITLE_RESOURCE_ID,
                    R.string.tab_selection_editor_deselect_all);
            layers.getDrawable(BACKGROUND)
                    .setLevel(
                            mContext.getResources().getInteger(R.integer.list_item_level_selected));
            layers.getDrawable(BACKGROUND)
                    .setTint(TabUiThemeProvider.getSelectionActionIconBackgroundColor(
                            mContext, isIncognito, allTabsSelected));

            layers.getDrawable(CHECKMARK).setAlpha(255);
            layers.getDrawable(CHECKMARK).setTint(
                    TabUiThemeProvider.getSelectionActionIconCheckedDrawableColor(
                            mContext, isIncognito));
            ((AnimatedVectorDrawableCompat) layers.getDrawable(CHECKMARK)).start();
        } else {
            assert false : "Invalid selection state";
        }
    }

    private static Drawable buildDrawable(Context context) {
        Drawable[] drawables = new Drawable[2];

        Drawable selectionListIcon = ResourcesCompat.getDrawable(context.getResources(),
                R.drawable.tab_grid_selection_list_icon, context.getTheme());
        drawables[BACKGROUND] = new InsetDrawable(selectionListIcon,
                (int) context.getResources().getDimension(
                        R.dimen.selection_tab_grid_toggle_button_inset));

        drawables[CHECKMARK] = AnimatedVectorDrawableCompat.create(
                context, R.drawable.ic_check_googblue_20dp_animated);
        return new LayerDrawable(drawables);
    }
}
