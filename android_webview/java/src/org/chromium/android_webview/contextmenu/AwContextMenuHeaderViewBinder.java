// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.contextmenu;

import static org.chromium.android_webview.contextmenu.AwContextMenuHeaderProperties.TITLE;

import android.text.TextUtils;
import android.view.View;
import android.widget.TextView;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

class AwContextMenuHeaderViewBinder {
    public static void bind(PropertyModel model, TextView textView, PropertyKey propertyKey) {
        if (propertyKey == TITLE) {
            textView.setText(model.get(TITLE));
            textView.setVisibility(TextUtils.isEmpty(model.get(TITLE)) ? View.GONE : View.VISIBLE);
        }
    }
}
