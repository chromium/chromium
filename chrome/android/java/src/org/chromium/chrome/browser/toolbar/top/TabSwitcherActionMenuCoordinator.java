// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.chromium.components.browser_ui.widget.listmenu.BasicListMenu.buildMenuDivider;
import static org.chromium.components.browser_ui.widget.listmenu.BasicListMenu.buildMenuListItem;

import android.content.Context;
import android.view.View;
import android.view.View.OnLongClickListener;
import android.widget.ListView;
import android.widget.PopupWindow;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.widget.listmenu.BasicListMenu;
import org.chromium.components.browser_ui.widget.listmenu.ListMenu;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuButton;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuButtonDelegate;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.widget.RectProvider;
import org.chromium.ui.widget.ViewRectProvider;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * The main coordinator for the Tab Switcher Action Menu, responsible for creating the popup menu
 * (popup window) in general and building a list of menu items.
 */
public class TabSwitcherActionMenuCoordinator {
    // For test.
    private View mContentView;

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({MenuItemType.DIVIDER, MenuItemType.CLOSE_TAB, MenuItemType.NEW_TAB,
            MenuItemType.NEW_INCOGNITO_TAB})
    public @interface MenuItemType {
        int DIVIDER = 0;
        int CLOSE_TAB = 1;
        int NEW_TAB = 2;
        int NEW_INCOGNITO_TAB = 3;
    }

    /**
     * @param onItemClicked  The clicked listener handling clicks on TabSwitcherActionMenu.
     * @return a long click listener of the long press action of tab switcher button.
     */
    public static OnLongClickListener createOnLongClickListener(Callback<Integer> onItemClicked) {
        return createOnLongClickListener(new TabSwitcherActionMenuCoordinator(), onItemClicked);
    }

    // internal helper function to create a long click listener.
    protected static OnLongClickListener createOnLongClickListener(
            TabSwitcherActionMenuCoordinator menu, Callback<Integer> onItemClicked) {
        return (view) -> {
            Context context = view.getContext();
            menu.displayMenu(context, (ListMenuButton) view, menu.buildMenuItems(), (id) -> {
                recordUserActions(id);
                onItemClicked.onResult(id);
            });
            return true;
        };
    }

    private static void recordUserActions(int id) {
        if (id == R.id.close_tab) {
            RecordUserAction.record("MobileMenuCloseTab.LongTapMenu");
        } else if (id == R.id.new_tab_menu_id) {
            RecordUserAction.record("MobileMenuNewTab.LongTapMenu");
        } else if (id == R.id.new_incognito_tab_menu_id) {
            RecordUserAction.record("MobileMenuNewIncognitoTab.LongTapMenu");
        }
    }

    /**
     * Created and display the tab switcher action menu anchored to the specified view.
     *
     * @param context        The context of the TabSwitcherActionMenu.
     * @param anchorView     The anchor {@link View} of the {@link PopupWindow}.
     * @param listItems      The menu item models.
     * @param onItemClicked  The clicked listener handling clicks on TabSwitcherActionMenu.
     */
    @VisibleForTesting
    public void displayMenu(final Context context, ListMenuButton anchorView, ModelList listItems,
            Callback<Integer> onItemClicked) {
        RectProvider rectProvider = getRectProvider(anchorView);
        BasicListMenu listMenu = new BasicListMenu(context, listItems, (model) -> {
            onItemClicked.onResult(model.get(ListMenuItemProperties.MENU_ITEM_ID));
        });

        mContentView = listMenu.getContentView();
        int verticalPadding = context.getResources().getDimensionPixelOffset(
                R.dimen.tab_switcher_menu_vertical_padding);
        ListView listView = listMenu.getListView();
        listView.setPaddingRelative(listView.getPaddingStart(), verticalPadding,
                listView.getPaddingEnd(), verticalPadding);
        ListMenuButtonDelegate delegate = new ListMenuButtonDelegate() {
            @Override
            public ListMenu getListMenu() {
                return listMenu;
            }

            @Override
            public RectProvider getRectProvider(View listMenuButton) {
                return rectProvider;
            }
        };

        anchorView.setDelegate(delegate);
        anchorView.showMenu();
    }

    protected RectProvider getRectProvider(View anchorView) {
        ViewRectProvider rectProvider = new ViewRectProvider(anchorView);
        rectProvider.setIncludePadding(true);

        int toolbarHeight = anchorView.getHeight();
        int iconHeight =
                anchorView.getResources().getDimensionPixelSize(R.dimen.toolbar_icon_height);
        int paddingBottom = (toolbarHeight - iconHeight) / 2;
        rectProvider.setInsetPx(0, 0, 0, paddingBottom);
        return rectProvider;
    }

    @VisibleForTesting
    public View getContentView() {
        return mContentView;
    }

    @VisibleForTesting
    public ModelList buildMenuItems() {
        ModelList itemList = new ModelList();
        itemList.add(buildListItemByMenuItemType(MenuItemType.CLOSE_TAB));
        itemList.add(buildListItemByMenuItemType(MenuItemType.DIVIDER));
        itemList.add(buildListItemByMenuItemType(MenuItemType.NEW_TAB));
        itemList.add(buildListItemByMenuItemType(MenuItemType.NEW_INCOGNITO_TAB));
        return itemList;
    }

    protected ListItem buildListItemByMenuItemType(@MenuItemType int type) {
        switch (type) {
            case MenuItemType.CLOSE_TAB:
                return buildMenuListItem(R.string.close_tab, R.id.close_tab, R.drawable.btn_close);
            case MenuItemType.NEW_TAB:
                return buildMenuListItem(
                        R.string.menu_new_tab, R.id.new_tab_menu_id, R.drawable.new_tab_icon);
            case MenuItemType.NEW_INCOGNITO_TAB:
                return buildMenuListItem(R.string.menu_new_incognito_tab,
                        R.id.new_incognito_tab_menu_id, R.drawable.incognito_simple);
            case MenuItemType.DIVIDER:
            default:
                return buildMenuDivider();
        }
    }
}
