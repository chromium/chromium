// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import static org.chromium.ui.listmenu.ContextMenuSubmenuItemProperties.ON_HOVER;
import static org.chromium.ui.listmenu.ContextMenuSubmenuItemProperties.TITLE;
import static org.chromium.ui.listmenu.ListMenuItemProperties.CLICK_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.ENABLED;

import android.view.View;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.ui.listmenu.ContextMenuSubmenuItemProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * View binder for a context menu item with submenu (of type {@code
 * ListItemType.CONTEXT_MENU_ITEM_WITH_SUBMENU}, with property keys {@link
 * ContextMenuSubmenuItemProperties}).
 */
@NullMarked
class ContextMenuItemWithSubmenuViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        TextView textView = view.findViewById(R.id.menu_row_text);
        if (propertyKey == TITLE) {
            textView.setText(model.get(TITLE));
        }  else if (propertyKey == ENABLED) {
            textView.setEnabled(model.get(ENABLED));
        } else if (propertyKey == ON_HOVER) {
            // TODO(crbug.com/424580483): Implement flyout submenus.
        } else if (propertyKey == CLICK_LISTENER) {
            view.setOnClickListener(model.get(CLICK_LISTENER));
        }
    }
    // MENU_ITEM_ID does not change the view.
}
