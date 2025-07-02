// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import static org.chromium.ui.listmenu.ListMenuItemProperties.CLICK_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.ENABLED;
import static org.chromium.ui.listmenu.ListMenuItemProperties.HOVER_LISTENER;
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
 * case of the share row which contains a quick share icon, and no start icon.
 */
@NullMarked
class ContextMenuItemViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        TextView textView = view.findViewById(R.id.menu_row_text);
        @Nullable ImageView startIcon = view.findViewById(R.id.menu_row_icon);
        boolean keepIconSpacing =
                model.containsKey(KEEP_START_ICON_SPACING_WHEN_HIDDEN)
                        && model.get(KEEP_START_ICON_SPACING_WHEN_HIDDEN);
        if (propertyKey == TITLE) {
            textView.setText(model.get(TITLE));
        } else if (propertyKey == CLICK_LISTENER) {
            view.setOnClickListener(model.get(CLICK_LISTENER));
        } else if (propertyKey == HOVER_LISTENER) {
            view.setOnHoverListener(model.get(HOVER_LISTENER));
        } else if (propertyKey == ENABLED) {
            view.setEnabled(model.get(ENABLED));
            textView.setEnabled(model.get(ENABLED));
            if (startIcon != null) startIcon.setEnabled(model.get(ENABLED));
        } else if (propertyKey == START_ICON_ID) {
            assert startIcon != null;
            int id = model.get(START_ICON_ID);
            Drawable drawable =
                    id == 0 ? null : AppCompatResources.getDrawable(view.getContext(), id);
            setStartIcon(startIcon, drawable, keepIconSpacing);
        } else if (propertyKey == START_ICON_DRAWABLE) {
            assert startIcon != null;
            Drawable drawable = model.get(START_ICON_DRAWABLE);
            setStartIcon(startIcon, drawable, keepIconSpacing);
        } else if (propertyKey == START_ICON_BITMAP) {
            assert startIcon != null;
            Bitmap bitmap = model.get(START_ICON_BITMAP);
            setStartIcon(
                    startIcon,
                    (bitmap == null ? null : new BitmapDrawable(view.getResources(), bitmap)),
                    keepIconSpacing);
        } else if (propertyKey == KEEP_START_ICON_SPACING_WHEN_HIDDEN) {
            assert startIcon != null;
            if (startIcon.getVisibility() != View.VISIBLE) {
                setStartIcon(startIcon, null, model.get(KEEP_START_ICON_SPACING_WHEN_HIDDEN));
            }
        } else if (propertyKey == ICON_TINT_COLOR_STATE_LIST_ID) {
            @ColorRes int tintColorId = model.get(ICON_TINT_COLOR_STATE_LIST_ID);
            if (tintColorId != 0) {
                ImageViewCompat.setImageTintList(
                        startIcon,
                        AppCompatResources.getColorStateList(
                                view.getContext(), model.get(ICON_TINT_COLOR_STATE_LIST_ID)));
            } else {
                // No tint.
                ImageViewCompat.setImageTintList(startIcon, null);
            }
        }
    }

    private static void setStartIcon(
            ImageView startIcon, @Nullable Drawable drawable, boolean keepStartIconSpacing) {
        startIcon.setImageDrawable(drawable);
        startIcon.setVisibility(
                drawable != null
                        // Icon is visible if it is set to a valid drawable.
                        ? View.VISIBLE
                        // Otherwise, make it invisible (blank) to keep the spacing, or remove it.
                        : (keepStartIconSpacing ? View.INVISIBLE : View.GONE));
    }
}
