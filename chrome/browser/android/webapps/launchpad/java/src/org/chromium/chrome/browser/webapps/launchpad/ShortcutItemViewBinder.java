// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.webapps.launchpad;

import android.graphics.Bitmap;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Class responsible for binding the model and the ShortcutItemView.
 */
class ShortcutItemViewBinder {
    static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == ShortcutItemProperties.NAME) {
            TextView titleText = view.findViewById(R.id.shortcut_name);
            titleText.setText(model.get(ShortcutItemProperties.NAME));
        } else if (propertyKey == ShortcutItemProperties.SHORTCUT_ICON) {
            Bitmap bitmap = model.get(ShortcutItemProperties.SHORTCUT_ICON);
            ImageView imageView = view.findViewById(R.id.shortcut_icon);
            imageView.setImageBitmap(bitmap);
            imageView.setVisibility(View.VISIBLE);
        } else if (propertyKey == ShortcutItemProperties.ON_CLICK) {
            view.setOnClickListener(model.get(ShortcutItemProperties.ON_CLICK));
        } else if (propertyKey == ShortcutItemProperties.HIDE_ICON) {
            view.findViewById(R.id.shortcut_icon).setVisibility(View.GONE);
        }
    }
}
