// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.ImageView;

import org.chromium.chrome.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

class RevampedContextMenuShareItemViewBinder extends RevampedContextMenuItemViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        RevampedContextMenuItemViewBinder.bind(
                model, view.findViewById(R.id.menu_row_text), propertyKey);
        if (propertyKey == RevampedContextMenuShareItemProperties.IMAGE) {
            Drawable drawable = model.get(RevampedContextMenuShareItemProperties.IMAGE);
            final ImageView imageView = view.findViewById(R.id.menu_row_share_icon);
            imageView.setImageDrawable(drawable);
            imageView.setVisibility(drawable != null ? View.VISIBLE : View.GONE);
            final int padding = view.getResources().getDimensionPixelSize(
                    R.dimen.revamped_context_menu_list_lateral_padding);
            // We don't need extra end padding for the text if the share icon is visible as the icon
            // already has padding.
            view.findViewById(R.id.menu_row_text)
                    .setPaddingRelative(padding, 0, drawable != null ? 0 : padding, 0);
        } else if (propertyKey == RevampedContextMenuShareItemProperties.CONTENT_DESC) {
            ((ImageView) view.findViewById(R.id.menu_row_share_icon))
                    .setContentDescription(view.getContext().getString(
                            R.string.accessibility_menu_share_via,
                            model.get(RevampedContextMenuShareItemProperties.CONTENT_DESC)));
        } else if (propertyKey == RevampedContextMenuShareItemProperties.CLICK_LISTENER) {
            view.findViewById(R.id.menu_row_share_icon)
                    .setOnClickListener(
                            model.get(RevampedContextMenuShareItemProperties.CLICK_LISTENER));
        }
    }
}
