// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top.tab_switcher_action_menu;

import android.view.View;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ChromeImageView;

/**
 * Class responsible for binding the model and the view.
 */
public class TabSwitcherActionMenuItemBinder {
    public static void binder(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == TabSwitcherActionMenuItemProperties.TITLE) {
            TextView textView = view.findViewById(R.id.menu_item_text);
            textView.setText(model.get(TabSwitcherActionMenuItemProperties.TITLE));
        } else if (propertyKey == TabSwitcherActionMenuItemProperties.ICON_ID) {
            ChromeImageView imageView = view.findViewById(R.id.menu_item_icon);
            imageView.setImageResource(model.get(TabSwitcherActionMenuItemProperties.ICON_ID));
        }
    }
}
