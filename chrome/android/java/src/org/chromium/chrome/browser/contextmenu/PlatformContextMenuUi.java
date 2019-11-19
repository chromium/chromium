// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.content.Context;
import android.text.TextUtils;
import android.util.Pair;
import android.view.ContextMenu;
import android.view.MenuItem;

import org.chromium.base.Callback;
import org.chromium.ui.base.WindowAndroid;

import java.util.List;

/**
 * A context menu that displays the Android Standard Context Menu. Comes the Item Groups into one
 * list displayed in the order received.
 */
public class PlatformContextMenuUi implements ContextMenuUi {
    private ContextMenu mMenu;

    public PlatformContextMenuUi(ContextMenu menu) {
        mMenu = menu;
    }

    @Override
    public void displayMenu(WindowAndroid window, ContextMenuParams params,
            List<Pair<Integer, List<ContextMenuItem>>> itemGroups, final Callback<Integer> listener,
            Runnable onMenuShown, Callback<Boolean> onMenuClosed) {
        Context context = window.getContext().get();
        String headerText = ChromeContextMenuPopulator.createHeaderText(params);
        if (!TextUtils.isEmpty(headerText)) {
            setHeaderText(context, mMenu, headerText);
        }

        MenuItem.OnMenuItemClickListener menuListener = new MenuItem.OnMenuItemClickListener() {
            @Override
            public boolean onMenuItemClick(MenuItem menuItem) {
                listener.onResult(menuItem.getItemId());
                return true;
            }
        };
        for (int groupIndex = 0; groupIndex < itemGroups.size(); groupIndex++) {
            List<ContextMenuItem> group = itemGroups.get(groupIndex).second;
            for (int itemIndex = 0; itemIndex < group.size(); itemIndex++) {
                ContextMenuItem item = group.get(itemIndex);
                MenuItem menuItem = mMenu.add(0, item.getMenuId(), 0, item.getTitle(context));
                menuItem.setOnMenuItemClickListener(menuListener);
            }
        }
    }

    private void setHeaderText(Context context, ContextMenu menu, String text) {
        ContextMenuTitleView title = new ContextMenuTitleView(context, text);
        menu.setHeaderView(title);
    }
}
