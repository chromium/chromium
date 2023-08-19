// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.res.ColorStateList;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.core.widget.ImageViewCompat;

import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.ListModelChangeProcessor;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyListModel;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Binds {@link TabSelectionEditorAction}'s {@link PropertyModel} to an
 * {@link TabSelectionEditorMenu} and {@link TabSelectionEditorMenuItem}'s {@link ListItem} to a
 * menu view.
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
            menu.add(actionModel.get(TabSelectionEditorActionProperties.MENU_ITEM_ID));
        }
        // Bind all properties.
        onItemsChanged(actionModels, menu, index, count, null);
        for (int i = index; i < index + count; i++) {
            PropertyModel actionModel = actionModels.get(i);
            menu.menuItemInitialized(
                    actionModel.get(TabSelectionEditorActionProperties.MENU_ITEM_ID));
        }
    }

    @Override
    public void onItemsRemoved(PropertyListModel<PropertyModel, PropertyKey> actionModels,
            TabSelectionEditorMenu menu, int index, int count) {
        if (actionModels.size() != 0) {
            throw new IllegalArgumentException("Partial removal of items is not supported");
        }
        menu.clear();
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
        for (PropertyKey key : TabSelectionEditorActionProperties.ACTION_KEYS) {
            bindMenuItemProperty(actionModel, menuItem, key);
        }
    }

    private void bindMenuItemProperty(
            PropertyModel actionModel, TabSelectionEditorMenuItem menuItem, PropertyKey key) {
        if (key == TabSelectionEditorActionProperties.TITLE_RESOURCE_ID) {
            updateTitle(actionModel, menuItem);
        } else if (key == TabSelectionEditorActionProperties.CONTENT_DESCRIPTION_RESOURCE_ID) {
            updateContentDescription(actionModel, menuItem);
        } else if (key == TabSelectionEditorActionProperties.ITEM_COUNT) {
            if (actionModel.get(TabSelectionEditorActionProperties.TITLE_IS_PLURAL)) {
                updateTitle(actionModel, menuItem);
            }
            updateContentDescription(actionModel, menuItem);
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
        } else if (key == TabSelectionEditorActionProperties.SHOULD_DISMISS_MENU) {
            menuItem.setShouldDismissMenu(
                    actionModel.get(TabSelectionEditorActionProperties.SHOULD_DISMISS_MENU));
        } else if (key == TabSelectionEditorActionProperties.ON_SELECTION_STATE_CHANGE) {
            menuItem.setOnSelectionStateChange(
                    actionModel.get(TabSelectionEditorActionProperties.ON_SELECTION_STATE_CHANGE));
        }
    }

    private void updateTitle(PropertyModel actionModel, TabSelectionEditorMenuItem menuItem) {
        int itemCount = actionModel.get(TabSelectionEditorActionProperties.TITLE_IS_PLURAL)
                ? actionModel.get(TabSelectionEditorActionProperties.ITEM_COUNT)
                : -1;
        menuItem.setTitle(
                actionModel.get(TabSelectionEditorActionProperties.TITLE_RESOURCE_ID), itemCount);
    }

    private void updateContentDescription(
            PropertyModel actionModel, TabSelectionEditorMenuItem menuItem) {
        menuItem.setContentDescription(
                actionModel.get(TabSelectionEditorActionProperties.CONTENT_DESCRIPTION_RESOURCE_ID),
                actionModel.get(TabSelectionEditorActionProperties.ITEM_COUNT));
    }

    public static void bindMenuItem(PropertyModel model, View view, PropertyKey propertyKey) {
        TextView textView = view.findViewById(R.id.menu_item_text);
        ImageView startIcon = view.findViewById(R.id.menu_item_icon);
        ImageView endIcon = view.findViewById(R.id.menu_item_end_icon);
        if (propertyKey == TabSelectionEditorActionProperties.TITLE) {
            textView.setText(model.get(TabSelectionEditorActionProperties.TITLE));
        } else if (propertyKey == TabSelectionEditorActionProperties.ICON) {
            startIcon.setImageDrawable(model.get(TabSelectionEditorActionProperties.ICON));
            startIcon.setVisibility(View.VISIBLE);
            endIcon.setVisibility(View.GONE);
        } else if (propertyKey == TabSelectionEditorActionProperties.ENABLED
                || propertyKey == TabSelectionEditorActionProperties.CONTENT_DESCRIPTION) {
            // Content description changes don't affect enabled state; however, enabled state
            // changes do affect content description. Updating enabled state is low-cost so
            // it can be updated regardless to minimize complexity.
            final boolean enabled = model.get(TabSelectionEditorActionProperties.ENABLED);
            view.setEnabled(enabled);
            textView.setEnabled(enabled);
            startIcon.setEnabled(enabled);
            endIcon.setEnabled(enabled);

            // Disabled state should just read out the text rather than the plural string details.
            if (enabled) {
                textView.setContentDescription(
                        model.get(TabSelectionEditorActionProperties.CONTENT_DESCRIPTION));
            } else {
                textView.setContentDescription(null);
            }
        } else if (propertyKey == TabSelectionEditorActionProperties.ICON_TINT) {
            ColorStateList colorStateList = model.get(TabSelectionEditorActionProperties.ICON_TINT);
            if (colorStateList != null) {
                ImageViewCompat.setImageTintList(startIcon, colorStateList);
            }
        }
    }
}
