// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top.tab_switcher_action_menu;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnLongClickListener;
import android.widget.ListView;
import android.widget.PopupWindow;

import androidx.annotation.DrawableRes;
import androidx.annotation.IdRes;
import androidx.annotation.IntDef;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.RectProvider;
import org.chromium.ui.widget.ViewRectProvider;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * The main coordinator for the Tab Switcher Action Menu, responsible for creating the popup menu
 * (popup window) in general and building a list of menu items.
 */
public class TabSwitcherActionMenuCoordinator {
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({ListItemType.DIVIDER, ListItemType.MENU_ITEM})
    public @interface ListItemType {
        int DIVIDER = 0;
        int MENU_ITEM = 1;
    }

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({MenuItemType.DIVIDER, MenuItemType.CLOSE_TAB, MenuItemType.NEW_TAB,
            MenuItemType.NEW_INCOGNITO_TAB})
    public @interface MenuItemType {
        int DIVIDER = 0;
        int CLOSE_TAB = 1;
        int NEW_TAB = 2;
        int NEW_INCOGNITO_TAB = 3;
    }

    private ListView mListView;
    private AnchoredPopupWindow mPopup;
    private View mContentView;

    /**
     * @param onItemClicked  The clicked listener handling clicks on TabSwitcherActionMenu
     * @return a long click listener of the long press action of tab switcher button
     */
    public static OnLongClickListener createOnLongClickListener(Callback<Integer> onItemClicked) {
        return createOnLongClickListener(new TabSwitcherActionMenuCoordinator(), onItemClicked);
    }

    // internal helper function to create a long click listener
    protected static OnLongClickListener createOnLongClickListener(
            TabSwitcherActionMenuCoordinator menu, Callback<Integer> onItemClicked) {
        return (view) -> {
            Context context = view.getContext();
            menu.displayMenu(context, view, menu.buildMenuItems(context), (id) -> {
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
     * Created and display the tab switcher action menu anchored to the specified view
     *
     * @param context        The context of the TabSwitcherActionMenu.
     * @param anchorView     The anchor {@link View} of the {@link PopupWindow}.
     * @param listItems      The menu item models
     * @param onItemClicked  The clicked listener handling clicks on TabSwitcherActionMenu
     */
    @VisibleForTesting
    public void displayMenu(final Context context, View anchorView, ModelList listItems,
            Callback<Integer> onItemClicked) {
        mContentView = LayoutInflater.from(context).inflate(
                R.layout.tab_switcher_action_menu_layout, null);
        mListView = (ListView) mContentView.findViewById(R.id.tab_switcher_action_menu_list);

        ModelListAdapter adapter = new ModelListAdapter(listItems) {
            @Override
            public boolean areAllItemsEnabled() {
                return false;
            }

            @Override
            public boolean isEnabled(int position) {
                // make divider not clickable
                return getItemViewType(position) != ListItemType.DIVIDER;
            }

            @Override
            public long getItemId(int position) {
                return ((ListItem) getItem(position))
                        .model.get(TabSwitcherActionMenuItemProperties.MENU_ID);
            }
        };

        mListView.setAdapter(adapter);

        // Note: clang-format does a bad job formatting lambdas so we turn it off here.
        // clang-format off
        adapter.registerType(ListItemType.DIVIDER,
                () -> LayoutInflater.from(mListView.getContext())
                        .inflate(R.layout.context_menu_divider, mListView, false),
                (m, v, p) -> {});

        adapter.registerType(ListItemType.MENU_ITEM,
                () -> LayoutInflater.from(mListView.getContext())
                        .inflate(R.layout.tab_switcher_action_menu_item, mListView, false),
                TabSwitcherActionMenuItemBinder::binder);
        // clang-format on

        mListView.setOnItemClickListener((p, v, pos, id) -> {
            if (onItemClicked != null) onItemClicked.onResult((int) id);
            mPopup.dismiss();
        });

        int popupWidth =
                context.getResources().getDimensionPixelSize(R.dimen.tab_switcher_menu_width);
        mPopup = new AnchoredPopupWindow(context, anchorView,
                ApiCompatibilityUtils.getDrawable(
                        context.getResources(), R.drawable.popup_bg_tinted),
                mContentView, getRectProvider(anchorView));
        mPopup.setFocusable(true);
        mPopup.setAnimationStyle(R.style.OverflowMenuAnim);

        mPopup.setMaxWidth(popupWidth);
        mPopup.setHorizontalOverlapAnchor(true);
        mPopup.show();
    }

    @VisibleForTesting
    public View getContentView() {
        return mContentView;
    }

    @VisibleForTesting
    public ModelList buildMenuItems(Context context) {
        ModelList itemList = new ModelList();
        itemList.add(buildListItemByMenuItemType(context, MenuItemType.CLOSE_TAB));
        itemList.add(buildListItemByMenuItemType(context, MenuItemType.DIVIDER));
        itemList.add(buildListItemByMenuItemType(context, MenuItemType.NEW_TAB));
        itemList.add(buildListItemByMenuItemType(context, MenuItemType.NEW_INCOGNITO_TAB));
        return itemList;
    }

    /**
     * Define how popup menu is positioned.
     * @param anchorView The view which the popup menu anchors.
     * @return Rect provider describing how to position the popup menu.
     */
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

    // internal helper function to build a list item given a menu item type
    protected ListItem buildListItemByMenuItemType(Context context, @MenuItemType int type) {
        switch (type) {
            case MenuItemType.CLOSE_TAB:
                return new ListItem(ListItemType.MENU_ITEM,
                        buildPropertyModel(
                                context, R.string.close_tab, R.id.close_tab, R.drawable.btn_close));
            case MenuItemType.NEW_TAB:
                return new ListItem(ListItemType.MENU_ITEM,
                        buildPropertyModel(context, R.string.menu_new_tab, R.id.new_tab_menu_id,
                                R.drawable.new_tab_icon));
            case MenuItemType.NEW_INCOGNITO_TAB:
                return new ListItem(ListItemType.MENU_ITEM,
                        buildPropertyModel(context, R.string.menu_new_incognito_tab,
                                R.id.new_incognito_tab_menu_id, R.drawable.incognito_simple));
            case MenuItemType.DIVIDER:
            default:
                return new ListItem(ListItemType.DIVIDER, new PropertyModel());
        }
    }

    protected PropertyModel buildPropertyModel(
            Context context, @StringRes int titleId, @IdRes int menuId, @DrawableRes int iconId) {
        return new PropertyModel.Builder(TabSwitcherActionMenuItemProperties.ALL_KEYS)
                .with(TabSwitcherActionMenuItemProperties.TITLE, context.getString(titleId))
                .with(TabSwitcherActionMenuItemProperties.MENU_ID, menuId)
                .with(TabSwitcherActionMenuItemProperties.ICON_ID, iconId)
                .build();
    }
}
