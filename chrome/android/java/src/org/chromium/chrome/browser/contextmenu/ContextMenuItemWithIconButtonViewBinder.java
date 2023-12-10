// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import static org.chromium.chrome.browser.contextmenu.ContextMenuItemWithIconButtonProperties.BUTTON_CLICK_LISTENER;
import static org.chromium.chrome.browser.contextmenu.ContextMenuItemWithIconButtonProperties.BUTTON_CONTENT_DESC;
import static org.chromium.chrome.browser.contextmenu.ContextMenuItemWithIconButtonProperties.BUTTON_IMAGE;

import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.ImageView;

import org.chromium.chrome.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

class ContextMenuItemWithIconButtonViewBinder extends ContextMenuItemViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        ContextMenuItemViewBinder.bind(model, view.findViewById(R.id.menu_row_text), propertyKey);
        if (propertyKey == BUTTON_IMAGE) {
            Drawable drawable = model.get(BUTTON_IMAGE);
            final ImageView imageView = view.findViewById(R.id.menu_row_share_icon);
            imageView.setImageDrawable(drawable);
            imageView.setVisibility(drawable != null ? View.VISIBLE : View.GONE);
            final int padding =
                    view.getResources()
                            .getDimensionPixelSize(R.dimen.context_menu_list_lateral_padding);
            // We don't need extra end padding for the text if the share icon is visible as the icon
            // already has padding.
            view.findViewById(R.id.menu_row_text)
                    .setPaddingRelative(padding, 0, drawable != null ? 0 : padding, 0);
        } else if (propertyKey == BUTTON_CONTENT_DESC) {
            ((ImageView) view.findViewById(R.id.menu_row_share_icon))
                    .setContentDescription(
                            view.getContext()
                                    .getString(
                                            R.string.accessibility_menu_share_via,
                                            model.get(BUTTON_CONTENT_DESC)));
        } else if (propertyKey == BUTTON_CLICK_LISTENER) {
            view.findViewById(R.id.menu_row_share_icon)
                    .setOnClickListener(model.get(BUTTON_CLICK_LISTENER));
        }
    }
}
