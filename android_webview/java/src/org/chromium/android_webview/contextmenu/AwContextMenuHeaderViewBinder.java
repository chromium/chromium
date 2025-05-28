// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.contextmenu;

import static org.chromium.android_webview.contextmenu.AwContextMenuHeaderProperties.TITLE;

import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.android_webview.R;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

@NullMarked
class AwContextMenuHeaderViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        TextView headerUrl = view.findViewById(R.id.menu_header_title);
        ImageView headerIcon = view.findViewById(R.id.menu_header_icon);

        if (propertyKey == TITLE) {
            headerUrl.setText(model.get(TITLE));
            headerUrl.setVisibility(TextUtils.isEmpty(model.get(TITLE)) ? View.GONE : View.VISIBLE);
        } else if (propertyKey == AwContextMenuHeaderProperties.HEADER_ICON) {
            Drawable drawable = model.get(AwContextMenuHeaderProperties.HEADER_ICON);
            if (drawable != null && headerIcon != null) {
                headerIcon.setImageDrawable(drawable);
                headerIcon.setVisibility(View.VISIBLE);
            }
        }
    }
}
