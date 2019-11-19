// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.view.View;
import android.widget.TextView;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

class RevampedContextMenuItemViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == RevampedContextMenuItemProperties.TEXT) {
            ((TextView) view).setText(model.get(RevampedContextMenuItemProperties.TEXT));
        } else if (propertyKey == RevampedContextMenuItemProperties.MENU_ID) {
        }
    }
}
