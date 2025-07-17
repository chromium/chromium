// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import static org.chromium.ui.listmenu.ContextMenuSubmenuItemProperties.SUBMENU_ITEMS;
import static org.chromium.ui.listmenu.ContextMenuSubmenuItemProperties.TITLE;
import static org.chromium.ui.listmenu.ListMenuItemProperties.CLICK_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.ENABLED;
import static org.chromium.ui.listmenu.ListMenuItemProperties.MENU_ITEM_ID;

import android.view.View;
import android.widget.ListView;

import androidx.annotation.NonNull;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.ui.listmenu.ContextMenuSubmenuHeaderItemProperties;
import org.chromium.ui.listmenu.ListItemType;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

@NullMarked
public class ContextMenuUtils {

    private static final int INVALID_ITEM_ID = -1;

    /**
     * Creates and configures a {@link ModelListAdapter} for the context menu.
     *
     * <p>This adapter handles different {@link ListItemType}s for context menu items, dividers, and
     * headers, and provides custom logic for determining item enabled status and retrieving item
     * IDs.
     *
     * @param listItems The {@link ModelList} containing the items to be displayed in the menu.
     * @return A configured {@link ModelListAdapter} ready to be set on the {@link ListView}.
     */
    @NonNull
    public static ModelListAdapter createAdapter(ModelList listItems) {
        ModelListAdapter adapter =
                new ModelListAdapter(listItems) {
                    @Override
                    public boolean areAllItemsEnabled() {
                        return false;
                    }

                    @Override
                    public boolean isEnabled(int position) {
                        int type = getItemViewType(position);
                        return type != ListItemType.DIVIDER && type != ListItemType.HEADER;
                    }

                    @Override
                    public long getItemId(int position) {
                        return isEnabled(position)
                                ? ((ListItem) getItem(position)).model.get(MENU_ITEM_ID)
                                : INVALID_ITEM_ID;
                    }
                };

        adapter.registerType(
                ListItemType.HEADER,
                new LayoutViewBuilder(R.layout.context_menu_header),
                ContextMenuHeaderViewBinder::bind);
        adapter.registerType(
                ListItemType.DIVIDER,
                new LayoutViewBuilder(R.layout.list_section_divider),
                (m, v, p) -> {});
        adapter.registerType(
                ListItemType.CONTEXT_MENU_ITEM,
                new LayoutViewBuilder(R.layout.context_menu_row),
                ContextMenuItemViewBinder::bind);
        adapter.registerType(
                ListItemType.CONTEXT_MENU_ITEM_WITH_ICON_BUTTON,
                new LayoutViewBuilder(R.layout.context_menu_row),
                ContextMenuItemViewBinder::bind);
        adapter.registerType(
                ListItemType.CONTEXT_MENU_ITEM_WITH_CHECKBOX,
                new LayoutViewBuilder<>(R.layout.context_menu_checkbox),
                ContextMenuItemWithCheckboxViewBinder::bind);
        adapter.registerType(
                ListItemType.CONTEXT_MENU_ITEM_WITH_RADIO_BUTTON,
                new LayoutViewBuilder<>(R.layout.context_menu_radio_button),
                ContextMenuItemWithRadioButtonViewBinder::bind);
        adapter.registerType(
                ListItemType.CONTEXT_MENU_ITEM_WITH_SUBMENU,
                new LayoutViewBuilder<>(R.layout.context_menu_submenu_parent_row),
                ContextMenuItemWithSubmenuViewBinder::bind);
        adapter.registerType(
                ListItemType.CONTEXT_MENU_SUBMENU_HEADER,
                new LayoutViewBuilder<>(R.layout.context_menu_submenu_header),
                ContextMenuItemWithSubmenuHeaderViewBinder::bind);

        return adapter;
    }

    /**
     * Sets up a submenu parent item in the context menu.
     *
     * @param modelList The {@link ModelList} to mutate.
     * @param item The {@link ListItem} to configure.
     * @param dismissDialog The {@link Runnable} to dismiss the dialog.
     */
    /* package */ static void setupSubmenuParent(
            ModelList modelList, ListItem item, Runnable dismissDialog) {
        item.model.set(CLICK_LISTENER, (unusedView) -> onSubmenuParentClick(modelList, item));
        setupCallbacksRecursively(modelList, item, dismissDialog);
    }

    /**
     * Callback to use when a menu item of type CONTEXT_MENU_ITEM_WITH_SUBMENU is clicked.
     *
     * @param modelList The {@link ModelList} to modify.
     * @param item The menu item which was clicked.
     */
    private static void onSubmenuParentClick(ModelList modelList, ListItem item) {
        ModelList parentModelList = shallowCopy(modelList);
        modelList.clear();
        // Add the clicked item as a header to the submenu.
        final PropertyModel model =
                new PropertyModel.Builder(ContextMenuSubmenuHeaderItemProperties.ALL_KEYS)
                        .with(ContextMenuSubmenuHeaderItemProperties.TITLE, item.model.get(TITLE))
                        .with(ENABLED, true)
                        .with(
                                CLICK_LISTENER,
                                (unusedView) -> setModelListContent(modelList, parentModelList))
                        .build();
        modelList.add(new ListItem(ListItemType.CONTEXT_MENU_SUBMENU_HEADER, model));

        for (ListItem listItem : item.model.get(SUBMENU_ITEMS)) {
            modelList.add(listItem);
        }
    }

    private static void setModelListContent(ModelList modelList, ModelList target) {
        modelList.clear();
        for (ListItem item : target) {
            modelList.add(item);
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
    /* package */ static boolean hasClickListener(ListItem item) {
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
    /* package */ static void addRunnableToCallback(ListItem item, Runnable dismissDialog) {
        if (hasClickListener(item)) {
            View.OnClickListener oldListener = item.model.get(CLICK_LISTENER);
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
     * @param modelList The {@link ModelList} to modify.
     * @param item The item to start with.
     * @param dismissDialog The {@link Runnable} to run.
     */
    public static void setupCallbacksRecursively(
            ModelList modelList, ListItem item, Runnable dismissDialog) {
        if (item.model.containsKey(SUBMENU_ITEMS)) {
            item.model.set(CLICK_LISTENER, (unusedView) -> onSubmenuParentClick(modelList, item));
            for (ListItem submenuItem :
                    PropertyModel.getFromModelOrDefault(item.model, SUBMENU_ITEMS, List.of())) {
                setupCallbacksRecursively(modelList, submenuItem, dismissDialog);
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
}
