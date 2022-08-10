// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.util.ArraySet;

import org.chromium.ui.modelutil.ListModelChangeProcessor;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyListModel;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Set;

/**
 * Binds an {@link TabSelectionEditorAction}'s {@link PropertyModel} to an
 * {@link TabSelectionEditorMenu}.
 */
public class TabSelectionEditorMenuAdapter implements ListModelChangeProcessor.ViewBinder<
        PropertyListModel<PropertyModel, PropertyKey>, TabSelectionEditorMenu, PropertyKey> {
    @Override
    public void onItemsInserted(PropertyListModel<PropertyModel, PropertyKey> actionModels,
            TabSelectionEditorMenu menu, int index, int count) {
        // TODO(ckitagawa): After initial configuration adding more items always results in items
        // being appended to the end of the menu. Realistically this should never occur, but
        // consider handling or asserting.
        for (int i = index; i < index + count; i++) {
            PropertyModel actionModel = actionModels.get(i);
            menu.add(actionModel.get(TabSelectionEditorActionProperties.MENU_ITEM_ID),
                    actionModel.get(TabSelectionEditorActionProperties.TITLE_RESOURCE_ID));
        }
        // Bind all properties.
        onItemsChanged(actionModels, menu, index, count, null);
    }

    @Override
    public void onItemsRemoved(PropertyListModel<PropertyModel, PropertyKey> actionModels,
            TabSelectionEditorMenu menu, int index, int count) {
        if (actionModels.size() == 0) {
            menu.clear();
            return;
        }

        // TODO(ckitagawa): Consider removing this as realisitically 1-by-1 removal should never
        // occur. Assert or throw an error instead.
        Set<Integer> menuIds = new ArraySet<>();
        for (PropertyModel actionModel : actionModels) {
            menuIds.add(actionModel.get(TabSelectionEditorActionProperties.MENU_ITEM_ID));
        }
        menu.keep(menuIds);
    }

    @Override
    public void onItemsChanged(PropertyListModel<PropertyModel, PropertyKey> actionModels,
            TabSelectionEditorMenu menu, int index, int count, PropertyKey key) {
        for (int i = index; i < index + count; i++) {
            onItemChanged(actionModels.get(i),
                    menu.getMenuItem(actionModels.get(i).get(
                            TabSelectionEditorActionProperties.MENU_ITEM_ID)),
                    key);
        }
    }

    private void onItemChanged(
            PropertyModel actionModel, TabSelectionEditorMenuItem menuItem, PropertyKey key) {
        if (key == null) {
            bindAllProperties(actionModel, menuItem);
            return;
        }
        bindMenuItemProperty(actionModel, menuItem, key);
    }

    private void bindAllProperties(PropertyModel actionModel, TabSelectionEditorMenuItem menuItem) {
        menuItem.initActionView(actionModel.get(TabSelectionEditorActionProperties.SHOW_MODE),
                actionModel.get(TabSelectionEditorActionProperties.BUTTON_TYPE));
        for (PropertyKey key : TabSelectionEditorActionProperties.ALL_KEYS) {
            bindMenuItemProperty(actionModel, menuItem, key);
        }
    }

    private void bindMenuItemProperty(
            PropertyModel actionModel, TabSelectionEditorMenuItem menuItem, PropertyKey key) {
        if (key == TabSelectionEditorActionProperties.TITLE_RESOURCE_ID) {
            menuItem.setTitleResourceId(
                    actionModel.get(TabSelectionEditorActionProperties.TITLE_RESOURCE_ID));
        } else if (key == TabSelectionEditorActionProperties.CONTENT_DESCRIPTION_RESOURCE_ID
                || key == TabSelectionEditorActionProperties.ITEM_COUNT) {
            menuItem.setContentDescription(
                    actionModel.get(
                            TabSelectionEditorActionProperties.CONTENT_DESCRIPTION_RESOURCE_ID),
                    actionModel.get(TabSelectionEditorActionProperties.ITEM_COUNT));
        } else if (key == TabSelectionEditorActionProperties.ICON_POSITION
                || key == TabSelectionEditorActionProperties.ICON) {
            menuItem.setIcon(actionModel.get(TabSelectionEditorActionProperties.ICON_POSITION),
                    actionModel.get(TabSelectionEditorActionProperties.ICON));
        } else if (key == TabSelectionEditorActionProperties.ENABLED) {
            menuItem.setEnabled(actionModel.get(TabSelectionEditorActionProperties.ENABLED));
        } else if (key == TabSelectionEditorActionProperties.TEXT_TINT) {
            menuItem.setTextTint(actionModel.get(TabSelectionEditorActionProperties.TEXT_TINT));
        } else if (key == TabSelectionEditorActionProperties.ICON_TINT) {
            menuItem.setIconTint(actionModel.get(TabSelectionEditorActionProperties.ICON_TINT));
        } else if (key == TabSelectionEditorActionProperties.ON_CLICK_LISTENER) {
            menuItem.setOnClickListener(
                    actionModel.get(TabSelectionEditorActionProperties.ON_CLICK_LISTENER));
        } else if (key == TabSelectionEditorActionProperties.ON_SELECTION_STATE_CHANGE) {
            menuItem.setOnSelectionStateChange(
                    actionModel.get(TabSelectionEditorActionProperties.ON_SELECTION_STATE_CHANGE));
        }
    }
}
