// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.view.View;

import androidx.annotation.DimenRes;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

/**
 * A coordinator for the menu in TabGridDialog toolbar. It is responsible for creating a list of
 * menu items, setting up the menu and displaying the menu.
 */
public class TabGridDialogMenuCoordinator extends TabGroupOverflowMenuCoordinator {
    /**
     * Creates a {@link View.OnClickListener} that creates the menu and shows it when clicked.
     *
     * @param onItemClicked The clicked listener callback that handles clicks on menu items.
     * @param isIncognito Whether the current tab group model filter is in an incognito state.
     * @param shouldShowDeleteGroup Whether to show the delete group option.
     * @return A {@link View.OnClickListener} for the button that opens up the menu.
     */
    static View.OnClickListener getTabGridDialogMenuOnClickListener(
            Callback<Integer> onItemClicked, boolean isIncognito, boolean shouldShowDeleteGroup) {
        return view -> {
            Context context = view.getContext();
            TabGridDialogMenuCoordinator menu =
                    new TabGridDialogMenuCoordinator(
                            context, view, onItemClicked, isIncognito, shouldShowDeleteGroup);
            menu.display();
        };
    }

    private TabGridDialogMenuCoordinator(
            Context context,
            View anchorView,
            Callback<Integer> onItemClicked,
            boolean isIncognito,
            boolean shouldShowDeleteGroup) {
        super(context, anchorView, onItemClicked, null, null, isIncognito, shouldShowDeleteGroup);
    }

    @Override
    protected ModelList buildMenuItems(boolean isIncognito) {
        ModelList itemList = new ModelList();
        itemList.add(
                BrowserUiListMenuUtils.buildMenuListItemWithIncognitoBranding(
                        R.string.menu_select_tabs,
                        R.id.select_tabs,
                        R.drawable.ic_select_check_box_24dp,
                        R.color.default_icon_color_light_tint_list,
                        R.style.TextAppearance_TextLarge_Primary_Baseline_Light,
                        isIncognito,
                        true));
        itemList.add(
                BrowserUiListMenuUtils.buildMenuListItemWithIncognitoBranding(
                        R.string.tab_grid_dialog_toolbar_edit_group_name,
                        R.id.edit_group_name,
                        R.drawable.material_ic_edit_24dp,
                        R.color.default_icon_color_light_tint_list,
                        R.style.TextAppearance_TextLarge_Primary_Baseline_Light,
                        isIncognito,
                        true));
        if (ChromeFeatureList.sTabGroupParityAndroid.isEnabled()) {
            itemList.add(
                    BrowserUiListMenuUtils.buildMenuListItemWithIncognitoBranding(
                            R.string.tab_grid_dialog_toolbar_edit_group_color,
                            R.id.edit_group_color,
                            R.drawable.ic_colorize_24dp,
                            R.color.default_icon_color_light_tint_list,
                            R.style.TextAppearance_TextLarge_Primary_Baseline_Light,
                            isIncognito,
                            true));
        }
        if (mShouldShowDeleteGroup && !isIncognito) {
            itemList.add(
                    BrowserUiListMenuUtils.buildMenuListItemWithIncognitoBranding(
                            R.string.tab_grid_dialog_toolbar_delete_group,
                            R.id.delete_tab,
                            R.drawable.material_ic_delete_24dp,
                            R.color.default_icon_color_light_tint_list,
                            R.style.TextAppearance_TextLarge_Primary_Baseline_Light,
                            isIncognito,
                            true));
        }
        return itemList;
    }

    @Override
    protected void runCallback(int menuId) {
        mOnItemClickedGridDialogCallback.onResult(menuId);
    }

    @Override
    protected @DimenRes int getMenuWidth() {
        return R.dimen.menu_width;
    }
}
