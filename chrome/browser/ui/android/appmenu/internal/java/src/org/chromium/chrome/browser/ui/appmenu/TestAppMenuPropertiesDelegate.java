// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import android.os.Bundle;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.ui.appmenu.test.R;

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

    @Override
    public int getAppMenuLayoutId() {
        return R.menu.test_menu;
    }

    @Nullable
    @Override
    public List<CustomViewBinder> getCustomViewBinders() {
        return null;
    }

    @Override
    public void prepareMenu(Menu menu, AppMenuHandler handler) {
        menu.findItem(R.id.menu_item_two).setEnabled(false);

        menu.findItem(R.id.icon_row_menu_id).setVisible(enableAppIconRow);
        if (enableAppIconRow) {
            menu.findItem(R.id.icon_one)
                    .setIcon(AppCompatResources.getDrawable(ContextUtils.getApplicationContext(),
                            R.drawable.test_ic_arrow_forward_black_24dp));
            menu.findItem(R.id.icon_two)
                    .setIcon(AppCompatResources.getDrawable(ContextUtils.getApplicationContext(),
                            R.drawable.test_ic_arrow_forward_black_24dp));
            menu.findItem(R.id.icon_three)
                    .setIcon(AppCompatResources.getDrawable(ContextUtils.getApplicationContext(),
                            R.drawable.test_ic_arrow_forward_black_24dp));
            menu.findItem(R.id.icon_three).setEnabled(false);
        }
    }

    @Nullable
    @Override
    public Bundle getBundleForMenuItem(MenuItem item) {
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

    @Override
    public void recordHighlightedMenuItemShown(Integer menuItemId) {}

    @Override
    public void recordHighlightedMenuItemClicked(Integer menuItemId) {}
}
