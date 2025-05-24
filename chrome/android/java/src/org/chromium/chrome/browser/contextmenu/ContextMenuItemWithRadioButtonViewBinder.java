// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import static org.chromium.ui.listmenu.ContextMenuRadioItemProperties.ENABLED;
import static org.chromium.ui.listmenu.ContextMenuRadioItemProperties.SELECTED;
import static org.chromium.ui.listmenu.ContextMenuRadioItemProperties.TITLE;

import android.view.View;
import android.widget.RadioButton;

import org.chromium.ui.listmenu.ContextMenuRadioItemProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * View binder for a context menu item with checkbox (of type {@code
 * ListItemType.CONTEXT_MENU_ITEM_WITH_RADIO_BUTTON}, with property keys {@link
 * ContextMenuRadioItemProperties}).
 */
class ContextMenuItemWithRadioButtonViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        RadioButton radioButton = (RadioButton) view.getRootView();
        if (propertyKey == TITLE) {
            radioButton.setText(model.get(TITLE));
        } else if (propertyKey == ENABLED) {
            radioButton.setEnabled(model.get(ENABLED));
        } else if (propertyKey == SELECTED) {
            radioButton.setChecked(model.get(SELECTED));
        }
        // MENU_ITEM_ID and ON_CLICK are used by the ContextMenuCoordinator.
        // Note that because this will be an item inside of a list, this view itself does not need
        // to implement the on-click behavior (it's handled by ContextMenuCoordinator).
    }
}
