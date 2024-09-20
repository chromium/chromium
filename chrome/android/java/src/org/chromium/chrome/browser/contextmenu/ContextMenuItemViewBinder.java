// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import static org.chromium.chrome.browser.contextmenu.ContextMenuItemProperties.ENABLED;
import static org.chromium.chrome.browser.contextmenu.ContextMenuItemProperties.TEXT;

import android.view.View;
import android.widget.TextView;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

class ContextMenuItemViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == TEXT) {
            ((TextView) view).setText(model.get(TEXT));
        } else if (propertyKey == ENABLED) {
            view.setEnabled(model.get(ENABLED));
        }
    }
}
