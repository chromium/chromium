// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import static org.chromium.ui.listmenu.ContextMenuCheckItemProperties.CHECKED;
import static org.chromium.ui.listmenu.ContextMenuCheckItemProperties.ENABLED;
import static org.chromium.ui.listmenu.ContextMenuCheckItemProperties.TITLE;

import android.view.View;
import android.widget.CheckBox;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.ui.listmenu.ContextMenuCheckItemProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * View binder for a context menu item with checkbox (of type {@code
 * ListItemType.CONTEXT_MENU_ITEM_WITH_CHECKBOX}, with property keys {@link
 * ContextMenuCheckItemProperties}).
 */
class ContextMenuItemWithCheckboxViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        CheckBox checkBox = view.findViewById(R.id.checkbox);
        TextView title = view.findViewById(R.id.checkbox_title);

        if (propertyKey == TITLE) {
            title.setText(model.get(TITLE));
        } else if (propertyKey == ENABLED) {
            checkBox.setEnabled(model.get(ENABLED));
            title.setEnabled(model.get(ENABLED));
        } else if (propertyKey == CHECKED) {
            checkBox.setChecked(model.get(CHECKED));
        }
        // MENU_ITEM_ID and ON_CLICK are used by the ContextMenuCoordinator.
        // Note that because this will be an item inside of a list, this view itself does not need
        // to implement the on-click behavior (it's handled by ContextMenuCoordinator).
    }
}
