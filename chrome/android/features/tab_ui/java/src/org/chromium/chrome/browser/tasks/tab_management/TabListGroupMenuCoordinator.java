// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.view.View;

import androidx.annotation.DimenRes;
import androidx.annotation.IdRes;

import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

/**
 * A coordinator for the menu on tab group cards in GTS. It is responsible for creating a list of
 * menu items, setting up the menu and displaying the menu.
 */
public class TabListGroupMenuCoordinator extends TabGroupOverflowMenuCoordinator {
    /** Helper interface for handling menu item clicks for tab group related actions. */
    @FunctionalInterface
    public interface OnItemClickedCallback {
        void onClick(@IdRes int menuId, int tabId);
    }

    /**
     * Creates a {@link TabListMediator.TabActionListener} that creates the menu and shows it when
     * clicked.
     *
     * @param onItemClicked The clicked listener callback that handles clicks on menu items.
     * @param tabId The tabId that represents which tab to perform the onItemClicked action on.
     * @param isIncognito Whether the current tab group model filter is in an incognito state.
     * @param shouldShowDeleteGroup Whether to show the delete group menu item.
     * @return A {@link TabListMediator.TabActionListener} for the button that opens up the menu.
     */
    static TabListMediator.TabActionListener getTabListGroupMenuOnClickListener(
            OnItemClickedCallback onItemClicked,
            int tabId,
            boolean isIncognito,
            boolean shouldShowDeleteGroup) {
        return (view, unusedTabId) -> {
            Context context = view.getContext();
            TabListGroupMenuCoordinator menu =
                    new TabListGroupMenuCoordinator(
                            context,
                            view,
                            onItemClicked,
                            tabId,
                            isIncognito,
                            shouldShowDeleteGroup);
            menu.display();
        };
    }

    private TabListGroupMenuCoordinator(
            Context context,
            View anchorView,
            OnItemClickedCallback onItemClicked,
            int tabId,
            boolean isIncognito,
            boolean shouldShowDeleteGroup) {
        super(context, anchorView, null, onItemClicked, tabId, isIncognito, shouldShowDeleteGroup);
    }

    @Override
    protected ModelList buildMenuItems(boolean isIncognito) {
        ModelList itemList = new ModelList();
        itemList.add(
                BrowserUiListMenuUtils.buildMenuListItemWithIncognitoText(
                        R.string.close_tab_group_menu_item,
                        R.id.close_tab,
                        R.style.TextAppearance_TextLarge_Primary_Baseline_Light,
                        isIncognito,
                        true));
        itemList.add(
                BrowserUiListMenuUtils.buildMenuListItemWithIncognitoText(
                        R.string.rename_tab_group_menu_item,
                        R.id.edit_group_name,
                        R.style.TextAppearance_TextLarge_Primary_Baseline_Light,
                        isIncognito,
                        true));
        itemList.add(
                BrowserUiListMenuUtils.buildMenuListItemWithIncognitoText(
                        R.string.ungroup_tab_group_menu_item,
                        R.id.ungroup_tab,
                        R.style.TextAppearance_TextLarge_Primary_Baseline_Light,
                        isIncognito,
                        true));
        // Delete does not make sense for incognito since the tab group is not saved to sync.
        if (mShouldShowDeleteGroup && !isIncognito) {
            itemList.add(
                    BrowserUiListMenuUtils.buildMenuListItemWithIncognitoText(
                            R.string.delete_tab_group_menu_item,
                            R.id.delete_tab,
                            R.style.TextAppearance_TextLarge_Primary_Baseline_Light,
                            isIncognito,
                            true));
        }
        return itemList;
    }

    @Override
    protected void runCallback(int menuId) {
        mOnItemClickedListGroupCallback.onClick(menuId, mTabId);
    }

    @Override
    protected @DimenRes int getMenuWidth() {
        return R.dimen.tab_group_menu_width;
    }
}
