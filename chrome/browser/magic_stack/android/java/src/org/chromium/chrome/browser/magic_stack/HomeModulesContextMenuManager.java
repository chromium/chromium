// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.magic_stack;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.components.browser_ui.widget.ListItemBuilder;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListMenu;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.AnchoredPopupWindow.VerticalOrientation;
import org.chromium.ui.widget.RectProvider;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** The manager class which handles showing the context menu on modules. */
@NullMarked
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

    private @Nullable ModuleDelegate mModuleDelegate;

    private @Nullable AnchoredPopupWindow mPopupWindow;

    /**
     * @param moduleDelegate The instance of magic stack {@link ModuleDelegate}.
     */
    public HomeModulesContextMenuManager(ModuleDelegate moduleDelegate) {
        mModuleDelegate = moduleDelegate;
    }

    public void destroy() {
        mModuleDelegate = null;
    }

    /**
     * Builds and displays context menu items for a magic stack module.
     *
     * @param view The magic stack module view that is long clicked to show the context menu.
     * @param moduleProvider The instance of magic stack {@link ModuleDelegate}.
     */
    public void displayMenu(View view, ModuleProvider moduleProvider) {
        if (mModuleDelegate == null) return;

        ListMenu menu =
                getListMenu(view, moduleProvider, mModuleDelegate, this::dismissPopupWindow);

        // No item added. We won't show the menu, so we can skip the rest.
        if (menu == null) return;

        mPopupWindow =
                new AnchoredPopupWindow(
                        view.getContext(),
                        view,
                        new ColorDrawable(Color.TRANSPARENT),
                        menu.getContentView(),
                        new RectProvider(getAnchorRectangle(view)));

        showContextMenu(menu, view);
        notifyContextMenuShown(moduleProvider);
    }

    @VisibleForTesting
    @Nullable BasicListMenu getListMenu(
            View view,
            ModuleProvider moduleProvider,
            ModuleDelegate moduleDelegate,
            Runnable dismissPopupWindowRunnable) {
        boolean hasItems = false;

        ModelList itemList = new ModelList();
        Context context = view.getContext();

        for (@ContextMenuItemId int itemId = 0; itemId < ContextMenuItemId.NUM_ENTRIES; itemId++) {
            if (!shouldShowItem(itemId, moduleProvider)) continue;

            if (itemId != ContextMenuItemId.HIDE_MODULE) {
                itemList.add(
                        new ListItemBuilder()
                                .withTitle(
                                        context.getString(
                                                getResourceIdForMenuItem(itemId, moduleProvider)))
                                .withMenuId(ContextMenuItemId.SHOW_CUSTOMIZE_SETTINGS)
                                .withIsTextEllipsizedAtEnd(true)
                                .build());
            } else {
                itemList.add(
                        new ListItemBuilder()
                                .withTitle(moduleProvider.getModuleContextMenuHideText(context))
                                .withMenuId(ContextMenuItemId.HIDE_MODULE)
                                .withIsTextEllipsizedAtEnd(true)
                                .build());
            }
            hasItems = true;
        }

        // No item added. We won't show the menu, so we can skip the rest.
        if (!hasItems) return null;

        ListMenu.Delegate delegate =
                (model, unusedView) -> {
                    switch (model.get(ListMenuItemProperties.MENU_ITEM_ID)) {
                        case ContextMenuItemId.HIDE_MODULE:
                            moduleDelegate.removeModuleAndDisable(moduleProvider.getModuleType());
                            HomeModulesMetricsUtils.recordContextMenuRemoveModule(
                                    moduleProvider.getModuleType());
                            break;
                        case ContextMenuItemId.SHOW_CUSTOMIZE_SETTINGS:
                            moduleDelegate.customizeSettings();
                            HomeModulesMetricsUtils.recordContextMenuCustomizeSettings(
                                    moduleProvider.getModuleType());
                            break;
                        default:
                            assert false : "Not reached.";
                    }
                    dismissPopupWindowRunnable.run();
                };

        return BrowserUiListMenuUtils.getBasicListMenu(context, itemList, delegate);
    }

    /**
     * Returns an anchor rectangle used by calculatePopupWindowSpec() in AnchoredPopupWindow class
     * to calculate the the absolute coordinates of the popup window with respect to the screen. The
     * horizontal position of the popup window should be anchorRect.left + (anchorRect.width() -
     * menu's width) / 2 + marginPx and the vertical position of the pop up window should be
     * anchorRect.y, when properties of the popup window are set as in showContextMenu().
     *
     * @param view The magic stack module view that is long clicked to show the context menu.
     */
    @VisibleForTesting
    Rect getAnchorRectangle(View view) {
        int[] coordinates = new int[2];
        view.getLocationOnScreen(coordinates);

        // The values of x and y are defined to display the menu at the center of clicked magic
        // stack module.
        int x = coordinates[0];
        int y = coordinates[1] + view.getHeight() / 4;

        return new Rect(x, y, x + view.getWidth(), y + view.getHeight());
    }

    @VisibleForTesting
    void showContextMenu(ListMenu menu, View view) {
        final View contentView = menu.getContentView();
        final int lateralPadding = contentView.getPaddingLeft() + contentView.getPaddingRight();

        assumeNonNull(mPopupWindow);
        AnchoredPopupWindow popupWindow = mPopupWindow;
        AnchoredPopupWindow.LayoutObserver layoutObserver =
                (positionBelow, x, y, width, height, rect) ->
                        popupWindow.setAnimationStyle(
                                positionBelow
                                        ? R.style.StartIconMenuAnim
                                        : R.style.StartIconMenuAnimBottom);
        mPopupWindow.setLayoutObserver(layoutObserver);
        mPopupWindow.setVerticalOverlapAnchor(true);
        // To fit the largest item of the menu in one line when it is possible,the width of the
        // mPopupWindow is the sum of the padding round the item and the max width of the menu item
        // or the width of the clicked magic stack module, if it is smaller.
        mPopupWindow.setDesiredContentWidth(
                Math.min(menu.getMaxItemWidth() + lateralPadding, view.getWidth()));
        mPopupWindow.setFocusable(true);
        mPopupWindow.setOutsideTouchable(true);

        // To place the menu at the center of the magic stack module, HorizontalOrientation.CENTER
        // and VerticalOrientation.BELOW are used.
        mPopupWindow.setPreferredHorizontalOrientation(
                AnchoredPopupWindow.HorizontalOrientation.CENTER);
        mPopupWindow.setPreferredVerticalOrientation(VerticalOrientation.BELOW);

        mPopupWindow.show();
    }

    /** Returns whether to show a context menu item. */
    @VisibleForTesting
    boolean shouldShowItem(@ContextMenuItemId int itemId, ModuleProvider moduleProvider) {
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
    private int getResourceIdForMenuItem(@ContextMenuItemId int id, ModuleProvider moduleProvider) {
        if (id == ContextMenuItemId.SHOW_CUSTOMIZE_SETTINGS) {
            return R.string.home_modules_context_menu_more_settings;
        }

        return moduleProvider.getResourceIdOfContextMenuItem(id);
    }

    /**
     * Called when the context menu is shown. It allows logging metrics about user actions.
     *
     * @param moduleProvider The module on which the context menu is shown.
     */
    private void notifyContextMenuShown(ModuleProvider moduleProvider) {
        moduleProvider.onContextMenuCreated();
        HomeModulesMetricsUtils.recordContextMenuShown(moduleProvider.getModuleType());
    }

    @VisibleForTesting
    void dismissPopupWindow() {
        if (mPopupWindow == null) return;

        mPopupWindow.dismiss();
        mPopupWindow = null;
    }

    public void setPopupWindowForTesting(AnchoredPopupWindow window) {
        AnchoredPopupWindow oldWindow = mPopupWindow;
        mPopupWindow = window;
        ResettersForTesting.register(() -> mPopupWindow = oldWindow);
    }
}
