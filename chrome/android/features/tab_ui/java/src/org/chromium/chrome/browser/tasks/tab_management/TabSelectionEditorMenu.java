// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.view.Menu;
import android.view.MenuItem;
import android.widget.Toolbar;

import androidx.collection.ArrayMap;
import androidx.collection.ArraySet;

import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;

import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * A view holder for a toolbar {@link Menu}. The {@code mMenu} contains a list of {@link MenuItem}s
 * from the {@link TabSelectionEditorMenuItem}s viewholder.
 */
public class TabSelectionEditorMenu
        implements Toolbar.OnMenuItemClickListener, SelectionDelegate.SelectionObserver<Integer> {
    private Context mContext;
    private Menu mMenu;
    private Map<Integer, TabSelectionEditorMenuItem> mMenuItems = new ArrayMap<>();

    /**
     * @param context to use for accessing resources.
     * @param menu the {@link Menu} to wrap.
     */
    public TabSelectionEditorMenu(Context context, Menu menu) {
        mContext = context;
        mMenu = menu;
    }

    /**
     * Add a {@link TabSelectionEditorMenuItem} and {@link MenuItem} to this menu.
     * @param menuItemId the {@link MenuItem} ID to use for this.
     * @param titleResourceId the resource ID to use for the title.
     */
    public void add(int menuItemId, int titleResourceId) {
        MenuItem menuItem = mMenu.add(/*groupId=*/0, menuItemId, Menu.NONE, titleResourceId);
        mMenuItems.put(menuItemId, new TabSelectionEditorMenuItem(mContext, menuItem));
    }

    /**
     * @param menuItemId the id of the item to get.
     * @return a {@link} TabSelectionEditorMenuItem or null if the key isn't present.
     */
    public TabSelectionEditorMenuItem getMenuItem(int menuItemId) {
        return mMenuItems.get(menuItemId);
    }

    /**
     * @param menuItemIds the menuItemIds of the items to keep; the rest will be removed.
     */
    public void keep(Set<Integer> menuItemIds) {
        Set<Integer> keysToRemove = new ArraySet<>(mMenuItems.keySet());
        keysToRemove.removeAll(menuItemIds);
        mMenuItems.keySet().removeAll(keysToRemove);
        for (int key : keysToRemove) {
            mMenu.removeItem(key);
        }
    }

    /**
     * Clears all items in the menu.
     */
    public void clear() {
        mMenu.clear();
        mMenuItems.clear();
    }

    /**
     * Delegates selection updates to each menu item.
     * @param selectedItems the currently selected items.
     */
    @Override
    public void onSelectionStateChange(List<Integer> selectedItems) {
        for (TabSelectionEditorMenuItem menuItem : mMenuItems.values()) {
            menuItem.onSelectionStateChange(selectedItems);
        }
    }

    @Override
    public boolean onMenuItemClick(MenuItem menuItem) {
        TabSelectionEditorMenuItem item = mMenuItems.get(menuItem.getItemId());
        if (item == null) return false;

        item.onClick();
        return true;
    }
}
