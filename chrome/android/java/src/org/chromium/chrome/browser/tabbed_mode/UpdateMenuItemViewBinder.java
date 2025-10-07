// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** A custom binder used to bind the update menu item. */
@NullMarked
class UpdateMenuItemViewBinder {
    /** Summary for the Update menu item. */
    public static final PropertyModel.WritableObjectPropertyKey<String> SUMMARY =
            new PropertyModel.WritableObjectPropertyKey<>("SUMMARY");

    /** The color to be applied to the title text. */
    public static final PropertyModel.WritableIntPropertyKey TITLE_COLOR_ID =
            new PropertyModel.WritableIntPropertyKey("TITLE_COLOR_ID");

    /** All the applicable property keys for the update menu item. */
    public static final PropertyKey[] ALL_KEYS =
            PropertyModel.concatKeys(
                    AppMenuItemProperties.ALL_KEYS, new PropertyKey[] {SUMMARY, TITLE_COLOR_ID});

    /** Handles binding the view and models changes. */
    public static void bind(PropertyModel model, View view, PropertyKey key) {
        if (key == AppMenuItemProperties.MENU_ITEM_ID) {
            int id = model.get(AppMenuItemProperties.MENU_ITEM_ID);
            assert id == R.id.update_menu_id;
            view.setId(id);
        } else if (key == SUMMARY) {
            TextView summary = view.findViewById(R.id.menu_item_summary);
            String summaryText = model.get(SUMMARY);
            if (!TextUtils.isEmpty(summaryText)) {
                summary.setText(summaryText);
                summary.setVisibility(View.VISIBLE);
            } else {
                summary.setText("");
                summary.setVisibility(View.GONE);
            }
        } else if (key == AppMenuItemProperties.TITLE) {
            TextView text = view.findViewById(R.id.menu_item_text);
            text.setText(model.get(AppMenuItemProperties.TITLE));
            text.setContentDescription(model.get(AppMenuItemProperties.TITLE));
        } else if (key == TITLE_COLOR_ID) {
            TextView text = view.findViewById(R.id.menu_item_text);
            text.setTextColor(
                    AppCompatResources.getColorStateList(
                            view.getContext(), model.get(TITLE_COLOR_ID)));
        } else if (key == AppMenuItemProperties.ICON) {
            ImageView image = view.findViewById(R.id.menu_item_icon);
            Drawable icon = model.get(AppMenuItemProperties.ICON);
            image.setVisibility(icon == null ? View.GONE : View.VISIBLE);
            image.setImageDrawable(icon);
        } else if (key == AppMenuItemProperties.ENABLED) {
            view.findViewById(R.id.menu_item_text)
                    .setEnabled(model.get(AppMenuItemProperties.ENABLED));
            view.setEnabled(model.get(AppMenuItemProperties.ENABLED));
        } else if (key == AppMenuItemProperties.CLICK_HANDLER) {
            view.setOnClickListener(
                    v -> model.get(AppMenuItemProperties.CLICK_HANDLER).onItemClick(model));
        }
    }

    /** Provides the minimum height for the view for menu sizing. */
    public static int getPixelHeight(Context context) {
        int textSize =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.overflow_menu_update_min_height);
        int paddingSize =
                context.getResources().getDimensionPixelSize(R.dimen.overflow_menu_update_padding);
        int iconSize =
                AppCompatResources.getDrawable(context, R.drawable.menu_update)
                        .getIntrinsicHeight();

        return Math.max(textSize, iconSize) + paddingSize * 2 /* top padding and bottom padding */;
    }
}
