// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.view.View;
import android.widget.TextView;

import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * ViewBinder for menu item in tab grid dialog menu.
 */
public class TabGridDialogMenuItemBinder {
    public static void binder(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == TabGridDialogMenuItemProperties.TITLE) {
            TextView textView = view.findViewById(R.id.menu_item_text);
            textView.setText(model.get(TabGridDialogMenuItemProperties.TITLE));
        }
    }
}
