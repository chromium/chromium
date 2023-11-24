// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.graphics.Bitmap;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

class ContextMenuHeaderViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == ContextMenuHeaderProperties.TITLE) {
            TextView titleText = view.findViewById(R.id.menu_header_title);
            titleText.setText(model.get(ContextMenuHeaderProperties.TITLE));
            titleText.setVisibility(
                    TextUtils.isEmpty(model.get(ContextMenuHeaderProperties.TITLE))
                            ? View.GONE
                            : View.VISIBLE);
        } else if (propertyKey == ContextMenuHeaderProperties.TITLE_MAX_LINES) {
            final int maxLines = model.get(ContextMenuHeaderProperties.TITLE_MAX_LINES);
            final TextView titleText = view.findViewById(R.id.menu_header_title);
            titleText.setMaxLines(maxLines);
            if (maxLines == Integer.MAX_VALUE) {
                titleText.setEllipsize(null);
            } else {
                titleText.setEllipsize(TextUtils.TruncateAt.END);
            }
        } else if (propertyKey == ContextMenuHeaderProperties.URL) {
            TextView urlText = view.findViewById(R.id.menu_header_url);
            urlText.setText(model.get(ContextMenuHeaderProperties.URL));
            urlText.setVisibility(
                    TextUtils.isEmpty(model.get(ContextMenuHeaderProperties.URL))
                            ? View.GONE
                            : View.VISIBLE);
        } else if (propertyKey == ContextMenuHeaderProperties.TITLE_AND_URL_CLICK_LISTENER) {
            view.findViewById(R.id.title_and_url)
                    .setOnClickListener(
                            model.get(ContextMenuHeaderProperties.TITLE_AND_URL_CLICK_LISTENER));
        } else if (propertyKey == ContextMenuHeaderProperties.URL_MAX_LINES) {
            final int maxLines = model.get(ContextMenuHeaderProperties.URL_MAX_LINES);
            final TextView urlText = view.findViewById(R.id.menu_header_url);
            urlText.setMaxLines(maxLines);
            if (maxLines == Integer.MAX_VALUE) {
                urlText.setEllipsize(null);
            } else {
                urlText.setEllipsize(TextUtils.TruncateAt.END);
            }
        } else if (propertyKey == ContextMenuHeaderProperties.IMAGE) {
            Bitmap bitmap = model.get(ContextMenuHeaderProperties.IMAGE);
            if (bitmap != null) {
                ImageView imageView = view.findViewById(R.id.menu_header_image);
                imageView.setImageBitmap(bitmap);
            }
        } else if (propertyKey == ContextMenuHeaderProperties.CIRCLE_BG_VISIBLE) {
            final boolean isVisible = model.get(ContextMenuHeaderProperties.CIRCLE_BG_VISIBLE);
            view.findViewById(R.id.circle_background)
                    .setVisibility(isVisible ? View.VISIBLE : View.INVISIBLE);
        } else if (propertyKey
                == ContextMenuHeaderProperties.OVERRIDE_HEADER_IMAGE_MAX_SIZE_PIXEL) {
            int maxSizeOverride =
                    model.get(ContextMenuHeaderProperties.OVERRIDE_HEADER_IMAGE_MAX_SIZE_PIXEL);
            if (ContextMenuHeaderProperties.INVALID_OVERRIDE != maxSizeOverride) {
                View image = view.findViewById(R.id.menu_header_image);
                LayoutParams lp = image.getLayoutParams();
                lp.width = maxSizeOverride;
                lp.height = maxSizeOverride;
                image.setLayoutParams(lp);
            }
        } else if (propertyKey
                == ContextMenuHeaderProperties.OVERRIDE_HEADER_CIRCLE_BG_SIZE_PIXEL) {
            int sizeOverride =
                    model.get(ContextMenuHeaderProperties.OVERRIDE_HEADER_CIRCLE_BG_SIZE_PIXEL);
            if (ContextMenuHeaderProperties.INVALID_OVERRIDE != sizeOverride) {
                View circleBg = view.findViewById(R.id.circle_background);
                LayoutParams lp = circleBg.getLayoutParams();
                lp.width = sizeOverride;
                lp.height = sizeOverride;
                circleBg.setLayoutParams(lp);
            }
        } else if (propertyKey
                == ContextMenuHeaderProperties.OVERRIDE_HEADER_CIRCLE_BG_MARGIN_PIXEL) {
            int marginOverride =
                    model.get(ContextMenuHeaderProperties.OVERRIDE_HEADER_CIRCLE_BG_MARGIN_PIXEL);
            if (ContextMenuHeaderProperties.INVALID_OVERRIDE != marginOverride) {
                View circleBg = view.findViewById(R.id.circle_background);
                MarginLayoutParams mlp = (MarginLayoutParams) circleBg.getLayoutParams();
                mlp.setMargins(marginOverride, marginOverride, marginOverride, marginOverride);
                circleBg.setLayoutParams(mlp);
            }
        }
    }
}
