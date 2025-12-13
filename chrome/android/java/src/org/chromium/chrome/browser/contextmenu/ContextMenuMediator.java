// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import static org.chromium.chrome.browser.contextmenu.ContextMenuItemWithIconButtonProperties.END_BUTTON_CLICK_LISTENER;
import static org.chromium.chrome.browser.contextmenu.ContextMenuItemWithIconButtonProperties.END_BUTTON_MENU_ID;
import static org.chromium.ui.listmenu.ListMenuItemProperties.CLICK_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.ENABLED;
import static org.chromium.ui.listmenu.ListMenuItemProperties.MENU_ITEM_ID;

import android.app.Activity;
import android.widget.ListView;

import androidx.annotation.IdRes;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.contextmenu.ContextMenuCoordinator.ContextMenuItemType;
import org.chromium.ui.hierarchicalmenu.HierarchicalMenuController;
import org.chromium.ui.listmenu.ListItemType;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Mediator for context menu; see {@link ContextMenuCoordinator}. */
@NullMarked
public class ContextMenuMediator {

    // This mediator mutates this ModelList so that the ListView it is attached to will be updated.
    private final ModelList mModelList = new ModelList();

    private final Activity mActivity;
    private final ContextMenuHeaderCoordinator mContextMenuHeaderCoordinator;
    private final Callback<Integer> mOnItemClicked;
    private final Runnable mDismissDialog;

    /**
     * Returns a mediator ({@link ContextMenuMediator}) to be used for a context menu. See {@link
     * ContextMenuCoordinator}.
     *
     * @param activity The parent {@link Activity}.
     * @param headerCoordinator The {@link ContextMenuHeaderCoordinator} to use.
     * @param onItemClicked A callback that takes the MENU_ITEM_ID of an item, to use on click.
     * @param dismissDialog The {@link Runnable} to use to dismiss the context menu.
     */
    /* package */ ContextMenuMediator(
            Activity activity,
            ContextMenuHeaderCoordinator headerCoordinator,
            Callback<Integer> onItemClicked,
            Runnable dismissDialog) {
        mActivity = activity;
        mContextMenuHeaderCoordinator = headerCoordinator;
        mOnItemClicked = onItemClicked;
        mDismissDialog = dismissDialog;
    }

    /**
     * Execute an action for the selected item and close the menu.
     *
     * @param id The id of the item. We expect this to be an ID returned by {@link
     *     ChromeContextMenuItem#getMenuId(int)}}.
     * @param enabled Whether the item is enabled.
     */
    private void clickItem(@IdRes int id, boolean enabled) {
        // Do not start any action when the activity is on the way to destruction.
        // See https://crbug.com/990987
        if (mActivity.isFinishing() || mActivity.isDestroyed()) return;

        mOnItemClicked.onResult(id);

        // Dismiss the dialog if the item is enabled.
        if (enabled) {
            mDismissDialog.run();
        }
    }

    /**
     * Configure the list pf items to show in the context menu. Mutates the ModelList so that the
     * {@link ListView} that uses it will be notified and update.
     *
     * @param items The input list of items (this method adds more, so it's not the final list).
     * @param hasHeader Whether the context menu list has a header item.
     * @param hierarchicalMenuController The {@link HierarchicalMenuController} to use.
     * @return The {@link ModelList} to show in the context menu.
     */
    /*package*/ ModelList updateAndGetModelList(
            List<ModelList> items,
            boolean hasHeader,
            HierarchicalMenuController hierarchicalMenuController) {

        mModelList.clear();

        // Start with the header
        if (hasHeader) {
            mModelList.add(
                    new ListItem(
                            ContextMenuItemType.HEADER, mContextMenuHeaderCoordinator.getModel()));
        }

        for (ModelList group : items) {
            // Add a divider if there are already items in the list.
            // (The first group should not have a divider above it.)
            if (!mModelList.isEmpty()) {
                mModelList.add(new ListItem(ListItemType.DIVIDER, new PropertyModel()));
            }

            // Add the items in the group. We must check for emptiness first, because addAll asserts
            // that its parameter contains at least one item.
            if (!group.isEmpty()) mModelList.addAll(group);
        }

        // Setup submenu navigation callbacks.
        hierarchicalMenuController.setupCallbacksRecursively(
                /* headerModelList= */ null, mModelList, mDismissDialog);

        // Add callbacks to all other first-level items.
        for (ListItem item : mModelList) {
            if (item.type == ListItemType.MENU_ITEM
                    || item.type == ContextMenuItemType.CONTEXT_MENU_ITEM_WITH_ICON_BUTTON) {
                // Note: this does NOT handle items inside submenus.
                item.model.set(
                        CLICK_LISTENER,
                        // Note: clickItem already includes dismissDialog.
                        (v) -> clickItem(item.model.get(MENU_ITEM_ID), item.model.get(ENABLED)));
            }
            if (item.type == ContextMenuItemType.CONTEXT_MENU_ITEM_WITH_ICON_BUTTON) {
                PropertyModel model = item.model;
                model.set(
                        END_BUTTON_CLICK_LISTENER,
                        (v) -> clickItem(model.get(END_BUTTON_MENU_ID), model.get(ENABLED)));
            }
        }

        return mModelList;
    }

    /* package= */ void clickItemForTesting(int id, boolean enabled) {
        clickItem(id, enabled);
    }
}
