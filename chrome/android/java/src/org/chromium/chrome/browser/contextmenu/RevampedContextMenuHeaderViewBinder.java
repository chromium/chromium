// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.graphics.Bitmap;
import android.text.TextUtils;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

class RevampedContextMenuHeaderViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == RevampedContextMenuHeaderProperties.TITLE) {
            TextView titleText = view.findViewById(R.id.menu_header_title);
            titleText.setText(model.get(RevampedContextMenuHeaderProperties.TITLE));
            titleText.setVisibility(
                    TextUtils.isEmpty(model.get(RevampedContextMenuHeaderProperties.TITLE))
                            ? View.GONE
                            : View.VISIBLE);
        } else if (propertyKey == RevampedContextMenuHeaderProperties.TITLE_MAX_LINES) {
            final int maxLines = model.get(RevampedContextMenuHeaderProperties.TITLE_MAX_LINES);
            final TextView titleText = view.findViewById(R.id.menu_header_title);
            titleText.setMaxLines(maxLines);
            if (maxLines == Integer.MAX_VALUE) {
                titleText.setEllipsize(null);
            } else {
                titleText.setEllipsize(TextUtils.TruncateAt.END);
            }
        } else if (propertyKey == RevampedContextMenuHeaderProperties.URL) {
            TextView urlText = view.findViewById(R.id.menu_header_url);
            urlText.setText(model.get(RevampedContextMenuHeaderProperties.URL));
            urlText.setVisibility(
                    TextUtils.isEmpty(model.get(RevampedContextMenuHeaderProperties.URL))
                            ? View.GONE
                            : View.VISIBLE);
        } else if (propertyKey
                == RevampedContextMenuHeaderProperties.TITLE_AND_URL_CLICK_LISTENER) {
            view.findViewById(R.id.title_and_url)
                    .setOnClickListener(model.get(
                            RevampedContextMenuHeaderProperties.TITLE_AND_URL_CLICK_LISTENER));
        } else if (propertyKey == RevampedContextMenuHeaderProperties.URL_MAX_LINES) {
            final int maxLines = model.get(RevampedContextMenuHeaderProperties.URL_MAX_LINES);
            final TextView urlText = view.findViewById(R.id.menu_header_url);
            urlText.setMaxLines(maxLines);
            if (maxLines == Integer.MAX_VALUE) {
                urlText.setEllipsize(null);
            } else {
                urlText.setEllipsize(TextUtils.TruncateAt.END);
            }
        } else if (propertyKey == RevampedContextMenuHeaderProperties.IMAGE) {
            Bitmap bitmap = model.get(RevampedContextMenuHeaderProperties.IMAGE);
            if (bitmap != null) {
                ImageView imageView = view.findViewById(R.id.menu_header_image);
                imageView.setImageBitmap(bitmap);
            }
        } else if (propertyKey == RevampedContextMenuHeaderProperties.CIRCLE_BG_VISIBLE) {
            final boolean isVisible =
                    model.get(RevampedContextMenuHeaderProperties.CIRCLE_BG_VISIBLE);
            view.findViewById(R.id.circle_background)
                    .setVisibility(isVisible ? View.VISIBLE : View.INVISIBLE);
        }
    }
}
