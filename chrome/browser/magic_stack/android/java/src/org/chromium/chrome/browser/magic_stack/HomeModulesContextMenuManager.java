// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.magic_stack;

import android.content.Context;
import android.graphics.Point;
import android.view.ContextMenu;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MenuItem.OnMenuItemClickListener;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** The manager class which handles showing the context menu on modules. */
public class HomeModulesContextMenuManager {
    /**
     * Types of context menu items which are shown when long pressing a module. Only two default
     * menu items are shown now for each module. To add a new menu item, please override {@link
     * ModuleProvider#isContextMenuItemSupported(int)} to make it return true for the module which
     * supports the new item.
     */
    @IntDef({
        ContextMenuItemId.HIDE_MODULE,
        ContextMenuItemId.SHOW_CUSTOMIZE_SETTINGS,
        ContextMenuItemId.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ContextMenuItemId {
        /** The "hide module" menu item is default shown on the context menu. */
        int HIDE_MODULE = 0;

        /** The "customize" menu item is default shown on the context menu. */
        int SHOW_CUSTOMIZE_SETTINGS = 1;

        int NUM_ENTRIES = 2;
    }

    private final Point mContextMenuStartPosition;

    private ModuleDelegate mModuleDelegate;

    /**
     * @param moduleDelegate The instance of magic stack {@link ModuleDelegate}.
     * @param startPosition The starting position to show the context menu.
     */
    public HomeModulesContextMenuManager(
            @NonNull ModuleDelegate moduleDelegate, @NonNull Point startPosition) {
        mModuleDelegate = moduleDelegate;
        mContextMenuStartPosition = startPosition;
    }

    public void destroy() {
        mModuleDelegate = null;
    }

    /**
     * Builds context menu items for a module.
     *
     * @param contextMenu The provided instance of {@link ContextMenu}.
     * @param associatedView The view to show the context menu.
     * @param moduleProvider The given module.
     */
    public void createContextMenu(
            @NonNull ContextMenu contextMenu,
            @NonNull View associatedView,
            @NonNull ModuleProvider moduleProvider) {
        if (mModuleDelegate == null) return;

        OnMenuItemClickListener listener =
                menuItem -> onMenuItemClickImpl(menuItem, moduleProvider);
        boolean hasItems = false;

        for (@ContextMenuItemId int itemId = 0; itemId < ContextMenuItemId.NUM_ENTRIES; itemId++) {
            if (!shouldShowItem(itemId, moduleProvider)) continue;

            if (itemId != ContextMenuItemId.HIDE_MODULE) {
                contextMenu
                        .add(
                                Menu.NONE,
                                itemId,
                                Menu.NONE,
                                getResourceIdForMenuItem(itemId, moduleProvider))
                        .setOnMenuItemClickListener(listener);
            } else {
                Context context = associatedView.getContext();
                contextMenu
                        .add(moduleProvider.getModuleContextMenuHideText(context))
                        .setOnMenuItemClickListener(listener);
            }
            hasItems = true;
        }

        // No item added. We won't show the menu, so we can skip the rest.
        if (!hasItems) return;

        notifyContextMenuShown(moduleProvider);
    }

    /**
     * Called when a context menu item is clicked.
     *
     * @param menuItem The menu item which is clicked.
     * @param moduleProvider The module which shows the context menu.
     * @return Whether the click on the menu item is handled.
     */
    boolean onMenuItemClickImpl(
            @NonNull MenuItem menuItem, @NonNull ModuleProvider moduleProvider) {
        switch (menuItem.getItemId()) {
            case ContextMenuItemId.HIDE_MODULE:
                mModuleDelegate.removeModuleAndDisable(moduleProvider.getModuleType());
                HomeModulesMetricsUtils.recordContextMenuRemoveModule(
                        moduleProvider.getModuleType());
                return true;
            case ContextMenuItemId.SHOW_CUSTOMIZE_SETTINGS:
                mModuleDelegate.customizeSettings();
                HomeModulesMetricsUtils.recordContextMenuCustomizeSettings(
                        moduleProvider.getModuleType());
                return true;
            default:
                assert false : "Not reached.";
                return false;
        }
    }

    /** Returns whether to show a context menu item. */
    @VisibleForTesting
    boolean shouldShowItem(@ContextMenuItemId int itemId, @NonNull ModuleProvider moduleProvider) {
        if (itemId == ContextMenuItemId.SHOW_CUSTOMIZE_SETTINGS
                || itemId == ContextMenuItemId.HIDE_MODULE) {
            return true;
        }

        return moduleProvider.isContextMenuItemSupported(itemId);
    }

    /**
     * Returns the resource id of the string name of a context menu item.
     *
     * @param id The id of the context menu item.
     */
    private int getResourceIdForMenuItem(
            @ContextMenuItemId int id, @NonNull ModuleProvider moduleProvider) {
        if (id == ContextMenuItemId.SHOW_CUSTOMIZE_SETTINGS) {
            return R.string.home_modules_context_menu_customize;
        }

        return moduleProvider.getResourceIdOfContextMenuItem(id);
    }

    /**
     * Called when the context menu is shown. It allows logging metrics about user actions.
     *
     * @param moduleProvider The module on which the context menu is shown.
     */
    private void notifyContextMenuShown(@NonNull ModuleProvider moduleProvider) {
        moduleProvider.onContextMenuCreated();
        HomeModulesMetricsUtils.recordContextMenuShown(moduleProvider.getModuleType());
    }

    /** Returns the starting position of the context menu. */
    Point getContextMenuOffset() {
        return mContextMenuStartPosition;
    }
}
