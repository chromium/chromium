// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import static org.chromium.chrome.browser.contextmenu.ContextMenuItemWithIconButtonProperties.END_BUTTON_CLICK_LISTENER;
import static org.chromium.chrome.browser.contextmenu.ContextMenuItemWithIconButtonProperties.END_BUTTON_CONTENT_DESC;
import static org.chromium.chrome.browser.contextmenu.ContextMenuItemWithIconButtonProperties.END_BUTTON_IMAGE;
import static org.chromium.ui.listmenu.ListMenuItemProperties.CLICK_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.ENABLED;
import static org.chromium.ui.listmenu.ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID;
import static org.chromium.ui.listmenu.ListMenuItemProperties.KEEP_START_ICON_SPACING_WHEN_HIDDEN;
import static org.chromium.ui.listmenu.ListMenuItemProperties.START_ICON_BITMAP;
import static org.chromium.ui.listmenu.ListMenuItemProperties.START_ICON_DRAWABLE;
import static org.chromium.ui.listmenu.ListMenuItemProperties.START_ICON_ID;
import static org.chromium.ui.listmenu.ListMenuItemProperties.TITLE;

import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.ColorRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.widget.ImageViewCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Class responsible for binding the model of the ListMenuItem and the view for context menu. Each
 * item is expected to have a text title and optionally a start icon. It also handles the special
 * case of the share row which contains a quick share icon. Note that start icon and quick share
 * icon should never appear at the same time.
 */
@NullMarked
class ContextMenuItemViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        View itemView = view.findViewById(R.id.menu_row_item);
        TextView textView = view.findViewById(R.id.menu_row_text);
        ImageView startIconView = view.findViewById(R.id.menu_row_icon);
        ImageView shareIconView = view.findViewById(R.id.menu_row_share_icon);
        boolean keepIconSpacing =
                model.containsKey(KEEP_START_ICON_SPACING_WHEN_HIDDEN)
                        && model.get(KEEP_START_ICON_SPACING_WHEN_HIDDEN);
        if (propertyKey == TITLE) {
            textView.setText(model.get(TITLE));
        } else if (propertyKey == CLICK_LISTENER) {
            itemView.setOnClickListener(model.get(CLICK_LISTENER));
        } else if (propertyKey == ENABLED) {
            view.setEnabled(model.get(ENABLED));
            itemView.setEnabled(model.get(ENABLED));
            textView.setEnabled(model.get(ENABLED));
            startIconView.setEnabled(model.get(ENABLED));
        } else if (propertyKey == START_ICON_ID) {
            int id = model.get(START_ICON_ID);
            Drawable drawable =
                    id == 0 ? null : AppCompatResources.getDrawable(view.getContext(), id);
            setIcon(startIconView, drawable, keepIconSpacing);
        } else if (propertyKey == START_ICON_DRAWABLE) {
            Drawable drawable = model.get(START_ICON_DRAWABLE);
            setIcon(startIconView, drawable, keepIconSpacing);
        } else if (propertyKey == START_ICON_BITMAP) {
            Bitmap bitmap = model.get(START_ICON_BITMAP);
            setIcon(
                    startIconView,
                    (bitmap == null ? null : new BitmapDrawable(view.getResources(), bitmap)),
                    keepIconSpacing);
        } else if (propertyKey == KEEP_START_ICON_SPACING_WHEN_HIDDEN) {
            if (startIconView.getVisibility() != View.VISIBLE) {
                setIcon(startIconView, null, keepIconSpacing);
            }
        } else if (propertyKey == ICON_TINT_COLOR_STATE_LIST_ID) {
            @ColorRes int tintColorId = model.get(ICON_TINT_COLOR_STATE_LIST_ID);
            if (tintColorId != 0) {
                ImageViewCompat.setImageTintList(
                        startIconView,
                        AppCompatResources.getColorStateList(
                                view.getContext(), model.get(ICON_TINT_COLOR_STATE_LIST_ID)));
            } else {
                // No tint.
                ImageViewCompat.setImageTintList(startIconView, null);
            }
        } else if (propertyKey == END_BUTTON_IMAGE) {
            Drawable drawable = model.get(END_BUTTON_IMAGE);
            setIcon(shareIconView, drawable, false);
        } else if (propertyKey == END_BUTTON_CONTENT_DESC) {
            shareIconView.setContentDescription(
                    view.getContext()
                            .getString(
                                    R.string.accessibility_menu_share_via,
                                    model.get(END_BUTTON_CONTENT_DESC)));
        } else if (propertyKey == END_BUTTON_CLICK_LISTENER) {
            shareIconView.setOnClickListener(model.get(END_BUTTON_CLICK_LISTENER));
        }

        assert (startIconView.getVisibility() != View.VISIBLE)
                        || (shareIconView.getVisibility() != View.VISIBLE)
                : "Start icon and share icon cannot be both visible";
    }

    private static void setIcon(
            ImageView imageView, @Nullable Drawable drawable, boolean keepSpacing) {
        imageView.setImageDrawable(drawable);
        imageView.setVisibility(
                drawable != null
                        // Icon is visible if it is set to a valid drawable.
                        ? View.VISIBLE
                        // Otherwise, make it invisible (blank) to keep the spacing, or remove it.
                        : (keepSpacing ? View.INVISIBLE : View.GONE));
    }
}
