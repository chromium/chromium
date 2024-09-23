// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.app.Activity;
import android.graphics.drawable.Drawable;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tinker_tank.TinkerTankDelegate;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

import java.util.List;

/** TinkerTank action for the {@link TabListEditorMenu}. */
public class TabListEditorTinkerTankAction extends TabListEditorAction {
    private Activity mActivity;

    /**
     * Create an action for share tabs to tinker tank.
     *
     * @param activity for loading resources.
     * @param showMode whether to show an action view.
     * @param buttonType the type of the action view.
     * @param iconPosition the position of the icon in the action view.
     */
    public static TabListEditorAction createAction(
            Activity activity,
            @ShowMode int showMode,
            @ButtonType int buttonType,
            @IconPosition int iconPosition) {
        assert TinkerTankDelegate.isEnabled();

        Drawable drawable =
                AppCompatResources.getDrawable(activity, R.drawable.ic_add_box_rounded_corner);
        return new TabListEditorTinkerTankAction(
                activity, showMode, buttonType, iconPosition, drawable);
    }

    private TabListEditorTinkerTankAction(
            Activity activity,
            @ShowMode int showMode,
            @ButtonType int buttonType,
            @IconPosition int iconPosition,
            Drawable drawable) {
        super(
                R.id.tab_list_editor_tinker_tank_menu_item,
                showMode,
                buttonType,
                iconPosition,
                R.string.menu_tinker_tank,
                null,
                drawable);
        mActivity = activity;
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
        assert !tabs.isEmpty() : "Tinker Tank action should not be enabled for no tabs.";
        BottomSheetController bottomSheetController =
                getActionDelegate().getBottomSheetController();

        if (bottomSheetController != null) {
            TinkerTankDelegate delegate = TinkerTankDelegate.create();
            delegate.maybeShowForSelectedTabs(mActivity, bottomSheetController, tabs);
        }
        return true;
    }

    @Override
    public boolean shouldHideEditorAfterAction() {
        return true;
    }
}
