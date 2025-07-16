// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import static org.chromium.chrome.browser.contextmenu.ContextMenuItemWithIconButtonProperties.END_BUTTON_CLICK_LISTENER;
import static org.chromium.chrome.browser.contextmenu.ContextMenuItemWithIconButtonProperties.END_BUTTON_MENU_ID;
import static org.chromium.ui.listmenu.ContextMenuSubmenuItemProperties.SUBMENU_ITEMS;
import static org.chromium.ui.listmenu.ContextMenuSubmenuItemProperties.TITLE;
import static org.chromium.ui.listmenu.ListMenuItemProperties.CLICK_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.ENABLED;
import static org.chromium.ui.listmenu.ListMenuItemProperties.MENU_ITEM_ID;

import android.app.Activity;
import android.view.View.OnClickListener;
import android.widget.ListView;

import androidx.annotation.IdRes;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.listmenu.ContextMenuSubmenuHeaderItemProperties;
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
    ContextMenuMediator(
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
     * @return The {@link ModelList} to show in the context menu.
     */
    /*package*/ ModelList updateAndGetModelList(List<ModelList> items, boolean hasHeader) {

        mModelList.clear();

        // Start with the header
        if (hasHeader) {
            mModelList.add(
                    new ListItem(ListItemType.HEADER, mContextMenuHeaderCoordinator.getModel()));
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

        for (ListItem item : mModelList) {
            // Special case handling (for items whose callbacks don't use clickItem method)
            if (hasClickListener(item)) {
                addRunnableToCallback(item, mDismissDialog);
                continue;
            }
            if (item.type == ListItemType.CONTEXT_MENU_ITEM_WITH_SUBMENU) {
                setupSubmenuParent(item, mDismissDialog);
                continue;
            }
            // Usual case handling
            if (item.type != ListItemType.DIVIDER && item.type != ListItemType.HEADER) {
                // Note: this does NOT handle items inside submenus.
                item.model.set(
                        CLICK_LISTENER,
                        (v) -> clickItem(item.model.get(MENU_ITEM_ID), item.model.get(ENABLED)));
            }
            if (item.type == ListItemType.CONTEXT_MENU_ITEM_WITH_ICON_BUTTON) {
                PropertyModel model = item.model;
                model.set(
                        END_BUTTON_CLICK_LISTENER,
                        (v) -> clickItem(model.get(END_BUTTON_MENU_ID), model.get(ENABLED)));
            }
        }

        return mModelList;
    }

    /**
     * Sets up a submenu parent item in the context menu.
     *
     * @param item The {@link ListItem} to configure.
     * @param dismissDialog The {@link Runnable} to dismiss the dialog.
     */
    private void setupSubmenuParent(ListItem item, Runnable dismissDialog) {
        item.model.set(CLICK_LISTENER, (unusedView) -> onSubmenuParentClick(item));
        setupCallbacksRecursively(item, dismissDialog);
    }

    /**
     * Callback to use when a menu item of type CONTEXT_MENU_ITEM_WITH_SUBMENU is clicked.
     *
     * @param item The menu item which was clicked.
     */
    private void onSubmenuParentClick(ListItem item) {
        ModelList parentModelList = shallowCopy(mModelList);
        mModelList.clear();
        // Add the clicked item as a header to the submenu.
        final PropertyModel model =
                new PropertyModel.Builder(ContextMenuSubmenuHeaderItemProperties.ALL_KEYS)
                        .with(ContextMenuSubmenuHeaderItemProperties.TITLE, item.model.get(TITLE))
                        .with(ENABLED, true)
                        .with(CLICK_LISTENER, (unusedView) -> setModelListContent(parentModelList))
                        .build();
        mModelList.add(new ListItem(ListItemType.CONTEXT_MENU_SUBMENU_HEADER, model));

        for (ListItem listItem : item.model.get(SUBMENU_ITEMS)) {
            mModelList.add(listItem);
        }
    }

    private void setModelListContent(ModelList target) {
        mModelList.clear();
        for (ListItem item : target) {
            mModelList.add(item);
        }
    }

    /** Returns a shallow copy of {@param modelList}. */
    private static ModelList shallowCopy(ModelList modelList) {
        ModelList result = new ModelList();
        for (ListItem item : modelList) {
            result.add(item);
        }
        return result;
    }

    /** Returns whether {@param item} has a click listener. */
    private static boolean hasClickListener(ListItem item) {
        return item.model != null
                && item.model.containsKey(CLICK_LISTENER)
                && item.model.get(CLICK_LISTENER) != null;
    }

    /**
     * Makes {@param dismissDialog} run at the end of the callback of {@param item}.
     *
     * @param item The item to which we would add {@param runnable}.
     * @param dismissDialog The {@link Runnable} to run to dismiss the dialog.
     */
    private static void addRunnableToCallback(ListItem item, Runnable dismissDialog) {
        if (hasClickListener(item)) {
            OnClickListener oldListener = item.model.get(CLICK_LISTENER);
            item.model.set(
                    CLICK_LISTENER,
                    (view) -> {
                        oldListener.onClick(view);
                        dismissDialog.run();
                    });
        }
    }

    /**
     * Runs {@param dismissDialog} at the end of each callback, recursively (through submenu items).
     *
     * @param item The item to start with.
     * @param dismissDialog The {@link Runnable} to run.
     */
    private void setupCallbacksRecursively(ListItem item, Runnable dismissDialog) {
        if (item.model.containsKey(SUBMENU_ITEMS)) {
            item.model.set(CLICK_LISTENER, (unusedView) -> onSubmenuParentClick(item));
            for (ListItem submenuItem :
                    PropertyModel.getFromModelOrDefault(item.model, SUBMENU_ITEMS, List.of())) {
                setupCallbacksRecursively(submenuItem, dismissDialog);
            }
        } else {
            // Note: CONTEXT_MENU_SUBMENU_HEADER items should be (and are) excluded by this,
            // because CONTEXT_MENU_SUBMENU_HEADER items aren't in the model's SUBMENU_ITEMS.
            // CONTEXT_MENU_ITEM_WITH_SUBMENU items should also not be included.
            // The rationale for excluding these is that we don't want to dismiss the dialog when we
            // are navigating through submenus.
            addRunnableToCallback(item, dismissDialog);
        }
    }

    /* package= */ void clickItemForTesting(int id, boolean enabled) {
        clickItem(id, enabled);
    }
}
