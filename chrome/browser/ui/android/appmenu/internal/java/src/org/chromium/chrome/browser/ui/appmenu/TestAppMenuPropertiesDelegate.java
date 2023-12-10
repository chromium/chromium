// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import android.os.Bundle;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.widget.PopupMenu;

import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler.AppMenuItemType;
import org.chromium.chrome.browser.ui.appmenu.test.R;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

class TestAppMenuPropertiesDelegate implements AppMenuPropertiesDelegate {
    public final CallbackHelper menuDismissedCallback = new CallbackHelper();
    public final CallbackHelper footerInflatedCallback = new CallbackHelper();
    public final CallbackHelper headerInflatedCallback = new CallbackHelper();
    public int footerResourceId;
    public int headerResourceId;
    public int groupDividerId;
    public boolean enableAppIconRow;
    public boolean iconBeforeItem;

    @Override
    public void destroy() {}

    @Nullable
    @Override
    public List<CustomViewBinder> getCustomViewBinders() {
        return null;
    }

    @Override
    public ModelList getMenuItems(
            CustomItemViewTypeProvider customItemViewTypeProvider, AppMenuHandler handler) {
        ModelList modelList = new ModelList();

        PopupMenu popup = new PopupMenu(ContextUtils.getApplicationContext(), null);
        Menu menu = popup.getMenu();
        MenuInflater inflater = popup.getMenuInflater();
        inflater.inflate(getAppMenuLayoutId(), menu);

        prepareMenu(menu, handler);
        for (int i = 0; i < menu.size(); ++i) {
            MenuItem item = menu.getItem(i);
            if (item.isVisible()) {
                PropertyModel propertyModel = AppMenuUtil.menuItemToPropertyModel(item);
                propertyModel.set(AppMenuItemProperties.POSITION, i);
                propertyModel.set(AppMenuItemProperties.SUPPORT_ENTER_ANIMATION, true);
                if (item.hasSubMenu()) {
                    ModelList subList = new ModelList();
                    for (int j = 0; j < item.getSubMenu().size(); ++j) {
                        MenuItem subitem = item.getSubMenu().getItem(j);
                        if (!subitem.isVisible()) continue;
                        PropertyModel subModel = AppMenuUtil.menuItemToPropertyModel(subitem);
                        subList.add(new MVCListAdapter.ListItem(0, subModel));
                    }
                    propertyModel.set(AppMenuItemProperties.SUBMENU, subList);
                }
                int menutype = AppMenuItemType.STANDARD;
                if (item.getItemId() == R.id.icon_row_menu_id) {
                    int viewCount = item.getSubMenu().size();
                    if (viewCount == 3) {
                        menutype = AppMenuItemType.THREE_BUTTON_ROW;
                    } else if (viewCount == 4) {
                        menutype = AppMenuItemType.FOUR_BUTTON_ROW;
                    } else if (viewCount == 5) {
                        menutype = AppMenuItemType.FIVE_BUTTON_ROW;
                    }
                }
                modelList.add(new MVCListAdapter.ListItem(menutype, propertyModel));
            }
        }

        return modelList;
    }

    @Override
    public void prepareMenu(Menu menu, AppMenuHandler handler) {
        menu.findItem(R.id.menu_item_two).setEnabled(false);

        menu.findItem(R.id.icon_row_menu_id).setVisible(enableAppIconRow);
        if (enableAppIconRow) {
            menu.findItem(R.id.icon_one)
                    .setIcon(
                            AppCompatResources.getDrawable(
                                    ContextUtils.getApplicationContext(),
                                    R.drawable.test_ic_arrow_forward_black_24dp));
            menu.findItem(R.id.icon_two)
                    .setIcon(
                            AppCompatResources.getDrawable(
                                    ContextUtils.getApplicationContext(),
                                    R.drawable.test_ic_arrow_forward_black_24dp));
            menu.findItem(R.id.icon_three)
                    .setIcon(
                            AppCompatResources.getDrawable(
                                    ContextUtils.getApplicationContext(),
                                    R.drawable.test_ic_arrow_forward_black_24dp));
            menu.findItem(R.id.icon_three).setEnabled(false);
        }
    }

    @Nullable
    @Override
    public Bundle getBundleForMenuItem(int itemId) {
        return null;
    }

    @Override
    public void loadingStateChanged(boolean isLoading) {}

    @Override
    public void onMenuDismissed() {
        menuDismissedCallback.notifyCalled();
    }

    @Override
    public int getFooterResourceId() {
        return footerResourceId;
    }

    @Override
    public int getHeaderResourceId() {
        return headerResourceId;
    }

    @Override
    public int getGroupDividerId() {
        return groupDividerId;
    }

    @Override
    public boolean shouldShowFooter(int maxMenuHeight) {
        return footerResourceId != 0;
    }

    @Override
    public boolean shouldShowHeader(int maxMenuHeight) {
        return headerResourceId != 0;
    }

    @Override
    public void onFooterViewInflated(AppMenuHandler appMenuHandler, View view) {
        footerInflatedCallback.notifyCalled();
    }

    @Override
    public void onHeaderViewInflated(AppMenuHandler appMenuHandler, View view) {
        headerInflatedCallback.notifyCalled();
    }

    @Override
    public boolean shouldShowIconBeforeItem() {
        return iconBeforeItem;
    }

    protected int getAppMenuLayoutId() {
        return R.menu.test_menu;
    }

    @Override
    public boolean isMenuIconAtStart() {
        return false;
    }

    @Override
    public void onMenuShown() {}
}
