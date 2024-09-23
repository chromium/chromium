// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
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
 * Binds {@link TabListEditorAction}'s {@link PropertyModel} to an {@link TabListEditorMenu} and
 * {@link TabListEditorMenuItem}'s {@link ListItem} to a menu view.
 */
public class TabListEditorMenuAdapter
        implements ListModelChangeProcessor.ViewBinder<
                PropertyListModel<PropertyModel, PropertyKey>, TabListEditorMenu, PropertyKey> {
    @Override
    public void onItemsInserted(
            PropertyListModel<PropertyModel, PropertyKey> actionModels,
            TabListEditorMenu menu,
            int index,
            int count) {
        // TODO(ckitagawa): After initial configuration adding more items always results in items
        // being appended to the end of the menu. Realistically this should never occur, but
        // consider handling or asserting.
        for (int i = index; i < index + count; i++) {
            PropertyModel actionModel = actionModels.get(i);
            menu.add(actionModel.get(TabListEditorActionProperties.MENU_ITEM_ID));
        }
        // Bind all properties.
        onItemsChanged(actionModels, menu, index, count, null);
        for (int i = index; i < index + count; i++) {
            PropertyModel actionModel = actionModels.get(i);
            menu.menuItemInitialized(actionModel.get(TabListEditorActionProperties.MENU_ITEM_ID));
        }
    }

    @Override
    public void onItemsRemoved(
            PropertyListModel<PropertyModel, PropertyKey> actionModels,
            TabListEditorMenu menu,
            int index,
            int count) {
        if (actionModels.size() != 0) {
            throw new IllegalArgumentException("Partial removal of items is not supported");
        }
        menu.clear();
    }

    @Override
    public void onItemsChanged(
            PropertyListModel<PropertyModel, PropertyKey> actionModels,
            TabListEditorMenu menu,
            int index,
            int count,
            PropertyKey key) {
        for (int i = index; i < index + count; i++) {
            onItemChanged(
                    actionModels.get(i),
                    menu.getMenuItem(
                            actionModels.get(i).get(TabListEditorActionProperties.MENU_ITEM_ID)),
                    key);
        }
    }

    private void onItemChanged(
            PropertyModel actionModel, TabListEditorMenuItem menuItem, PropertyKey key) {
        if (key == null) {
            bindAllProperties(actionModel, menuItem);
            return;
        }
        bindMenuItemProperty(actionModel, menuItem, key);
    }

    private void bindAllProperties(PropertyModel actionModel, TabListEditorMenuItem menuItem) {
        menuItem.initActionView(
                actionModel.get(TabListEditorActionProperties.SHOW_MODE),
                actionModel.get(TabListEditorActionProperties.BUTTON_TYPE));
        for (PropertyKey key : TabListEditorActionProperties.ACTION_KEYS) {
            bindMenuItemProperty(actionModel, menuItem, key);
        }
    }

    private void bindMenuItemProperty(
            PropertyModel actionModel, TabListEditorMenuItem menuItem, PropertyKey key) {
        if (key == TabListEditorActionProperties.TITLE_RESOURCE_ID) {
            updateTitle(actionModel, menuItem);
        } else if (key == TabListEditorActionProperties.CONTENT_DESCRIPTION_RESOURCE_ID) {
            updateContentDescription(actionModel, menuItem);
        } else if (key == TabListEditorActionProperties.ITEM_COUNT) {
            if (actionModel.get(TabListEditorActionProperties.TITLE_IS_PLURAL)) {
                updateTitle(actionModel, menuItem);
            }
            updateContentDescription(actionModel, menuItem);
        } else if (key == TabListEditorActionProperties.ICON_POSITION
                || key == TabListEditorActionProperties.ICON) {
            menuItem.setIcon(
                    actionModel.get(TabListEditorActionProperties.ICON_POSITION),
                    actionModel.get(TabListEditorActionProperties.ICON));
        } else if (key == TabListEditorActionProperties.ENABLED) {
            menuItem.setEnabled(actionModel.get(TabListEditorActionProperties.ENABLED));
        } else if (key == TabListEditorActionProperties.TEXT_APPEARANCE_ID) {
            menuItem.setTextAppearance(
                    actionModel.get(TabListEditorActionProperties.TEXT_APPEARANCE_ID));
        } else if (key == TabListEditorActionProperties.TEXT_TINT) {
            menuItem.setTextTint(actionModel.get(TabListEditorActionProperties.TEXT_TINT));
        } else if (key == TabListEditorActionProperties.ICON_TINT) {
            menuItem.setIconTint(actionModel.get(TabListEditorActionProperties.ICON_TINT));
        } else if (key == TabListEditorActionProperties.ON_CLICK_LISTENER) {
            menuItem.setOnClickListener(
                    actionModel.get(TabListEditorActionProperties.ON_CLICK_LISTENER));
        } else if (key == TabListEditorActionProperties.SHOULD_DISMISS_MENU) {
            menuItem.setShouldDismissMenu(
                    actionModel.get(TabListEditorActionProperties.SHOULD_DISMISS_MENU));
        } else if (key == TabListEditorActionProperties.ON_SELECTION_STATE_CHANGE) {
            menuItem.setOnSelectionStateChange(
                    actionModel.get(TabListEditorActionProperties.ON_SELECTION_STATE_CHANGE));
        }
    }

    private void updateTitle(PropertyModel actionModel, TabListEditorMenuItem menuItem) {
        int itemCount =
                actionModel.get(TabListEditorActionProperties.TITLE_IS_PLURAL)
                        ? actionModel.get(TabListEditorActionProperties.ITEM_COUNT)
                        : -1;
        menuItem.setTitle(
                actionModel.get(TabListEditorActionProperties.TITLE_RESOURCE_ID), itemCount);
    }

    private void updateContentDescription(
            PropertyModel actionModel, TabListEditorMenuItem menuItem) {
        menuItem.setContentDescription(
                actionModel.get(TabListEditorActionProperties.CONTENT_DESCRIPTION_RESOURCE_ID),
                actionModel.get(TabListEditorActionProperties.ITEM_COUNT));
    }

    public static void bindMenuItem(PropertyModel model, View view, PropertyKey propertyKey) {
        TextView textView = view.findViewById(R.id.menu_item_text);
        ImageView startIcon = view.findViewById(R.id.menu_item_icon);
        ImageView endIcon = view.findViewById(R.id.menu_item_end_icon);
        if (propertyKey == TabListEditorActionProperties.TITLE) {
            textView.setText(model.get(TabListEditorActionProperties.TITLE));
        } else if (propertyKey == TabListEditorActionProperties.ICON) {
            Drawable icon = model.get(TabListEditorActionProperties.ICON);
            startIcon.setImageDrawable(icon);
            startIcon.setVisibility(icon == null ? View.GONE : View.VISIBLE);
            endIcon.setVisibility(View.GONE);
        } else if (propertyKey == TabListEditorActionProperties.ENABLED
                || propertyKey == TabListEditorActionProperties.CONTENT_DESCRIPTION) {
            // Content description changes don't affect enabled state; however, enabled state
            // changes do affect content description. Updating enabled state is low-cost so
            // it can be updated regardless to minimize complexity.
            final boolean enabled = model.get(TabListEditorActionProperties.ENABLED);
            view.setEnabled(enabled);
            textView.setEnabled(enabled);
            startIcon.setEnabled(enabled);
            endIcon.setEnabled(enabled);

            // Disabled state should just read out the text rather than the plural string details.
            if (enabled) {
                textView.setContentDescription(
                        model.get(TabListEditorActionProperties.CONTENT_DESCRIPTION));
            } else {
                textView.setContentDescription(null);
            }
        } else if (propertyKey == TabListEditorActionProperties.TEXT_APPEARANCE_ID) {
            textView.setTextAppearance(model.get(TabListEditorActionProperties.TEXT_APPEARANCE_ID));
        } else if (propertyKey == TabListEditorActionProperties.ICON_TINT) {
            ColorStateList colorStateList = model.get(TabListEditorActionProperties.ICON_TINT);
            if (colorStateList != null) {
                ImageViewCompat.setImageTintList(startIcon, colorStateList);
            }
        }
    }
}
