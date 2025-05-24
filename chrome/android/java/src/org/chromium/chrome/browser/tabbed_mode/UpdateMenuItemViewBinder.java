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
import androidx.core.graphics.drawable.DrawableCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.toolbar.menu_button.MenuItemState;
import org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties;
import org.chromium.chrome.browser.ui.appmenu.AppMenuUtil;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** A custom binder used to bind the update menu item. */
@NullMarked
class UpdateMenuItemViewBinder {
    /** Handles binding the view and models changes. */
    public static void bind(PropertyModel model, View view, PropertyKey key) {
        AppMenuUtil.bindStandardItemEnterAnimation(model, view, key);

        @Nullable MenuItemState itemState =
                ((MenuItemState) model.get(AppMenuItemProperties.CUSTOM_ITEM_DATA));

        if (key == AppMenuItemProperties.MENU_ITEM_ID) {
            int id = model.get(AppMenuItemProperties.MENU_ITEM_ID);
            assert id == R.id.update_menu_id;
            view.setId(id);

            if (itemState != null) {
                TextView summary = view.findViewById(R.id.menu_item_summary);
                if (!TextUtils.isEmpty(itemState.summary)) {
                    summary.setText(itemState.summary);
                    summary.setVisibility(View.VISIBLE);
                } else {
                    summary.setText("");
                    summary.setVisibility(View.GONE);
                }
            }
        } else if (key == AppMenuItemProperties.TITLE) {
            TextView text = view.findViewById(R.id.menu_item_text);
            if (itemState == null) {
                text.setText(model.get(AppMenuItemProperties.TITLE));
            } else {
                text.setText(itemState.title);
                text.setTextColor(
                        AppCompatResources.getColorStateList(
                                view.getContext(), itemState.titleColorId));
            }
        } else if (key == AppMenuItemProperties.TITLE_CONDENSED) {
            TextView text = view.findViewById(R.id.menu_item_text);
            if (itemState == null) {
                CharSequence titleCondensed = model.get(AppMenuItemProperties.TITLE_CONDENSED);
                text.setContentDescription(titleCondensed);
            } else {
                text.setContentDescription(view.getResources().getString(itemState.title));
            }
        } else if (key == AppMenuItemProperties.ICON) {
            ImageView image = view.findViewById(R.id.menu_item_icon);

            if (itemState == null) {
                Drawable icon = model.get(AppMenuItemProperties.ICON);
                image.setImageDrawable(icon);
                image.setVisibility(View.VISIBLE);
                return;
            }

            image.setImageResource(itemState.icon);
            if (itemState.iconTintId != 0) {
                DrawableCompat.setTint(
                        image.getDrawable(), view.getContext().getColor(itemState.iconTintId));
            }
        } else if (key == AppMenuItemProperties.ENABLED) {
            view.findViewById(R.id.menu_item_text)
                    .setEnabled(model.get(AppMenuItemProperties.ENABLED));
            if (itemState != null) view.setEnabled(itemState.enabled);
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
