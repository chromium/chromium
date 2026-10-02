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

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.ColorRes;
import androidx.annotation.VisibleForTesting;
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
    @VisibleForTesting
    static class ViewHolder {
        private final View mItemView;
        private final TextView mTextView;
        private final ImageView mStartIconView;
        private final ImageView mShareIconView;

        private ViewHolder(View view) {
            mItemView = view.findViewById(R.id.menu_row_item);
            mTextView = view.findViewById(R.id.menu_row_text);
            mStartIconView = view.findViewById(R.id.menu_row_icon);
            mShareIconView = view.findViewById(R.id.menu_row_share_icon);
        }
    }

    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        ViewHolder holder = (ViewHolder) view.getTag(R.id.context_menu_item_view_holder);
        if (holder == null) {
            holder = new ViewHolder(view);
            view.setTag(R.id.context_menu_item_view_holder, holder);
        }

        boolean keepIconSpacing =
                model.containsKey(KEEP_START_ICON_SPACING_WHEN_HIDDEN)
                        && model.get(KEEP_START_ICON_SPACING_WHEN_HIDDEN);
        if (propertyKey == TITLE) {
            holder.mTextView.setText(model.get(TITLE));
        } else if (propertyKey == CLICK_LISTENER) {
            holder.mItemView.setOnClickListener(model.get(CLICK_LISTENER));
        } else if (propertyKey == ENABLED) {
            boolean enabled = model.get(ENABLED);
            view.setEnabled(enabled);
            holder.mItemView.setEnabled(enabled);
            holder.mTextView.setEnabled(enabled);
            holder.mStartIconView.setEnabled(enabled);
        } else if (propertyKey == START_ICON_ID) {
            int id = model.get(START_ICON_ID);
            Drawable drawable =
                    id == 0 ? null : AppCompatResources.getDrawable(view.getContext(), id);
            setIcon(holder.mStartIconView, drawable, keepIconSpacing);
        } else if (propertyKey == START_ICON_DRAWABLE) {
            Drawable drawable = model.get(START_ICON_DRAWABLE);
            setIcon(holder.mStartIconView, drawable, keepIconSpacing);
        } else if (propertyKey == START_ICON_BITMAP) {
            Bitmap bitmap = model.get(START_ICON_BITMAP);
            setIcon(
                    holder.mStartIconView,
                    (bitmap == null ? null : new BitmapDrawable(view.getResources(), bitmap)),
                    keepIconSpacing);
        } else if (propertyKey == KEEP_START_ICON_SPACING_WHEN_HIDDEN) {
            if (holder.mStartIconView.getVisibility() != View.VISIBLE) {
                setIcon(holder.mStartIconView, /* drawable= */ null, keepIconSpacing);
            }
        } else if (propertyKey == ICON_TINT_COLOR_STATE_LIST_ID) {
            @ColorRes int tintColorId = model.get(ICON_TINT_COLOR_STATE_LIST_ID);
            if (tintColorId != Resources.ID_NULL) {
                ImageViewCompat.setImageTintList(
                        holder.mStartIconView, view.getContext().getColorStateList(tintColorId));
            } else {
                // No tint.
                ImageViewCompat.setImageTintList(holder.mStartIconView, /* tint= */ null);
            }
        } else if (propertyKey == END_BUTTON_IMAGE) {
            Drawable drawable = model.get(END_BUTTON_IMAGE);
            setIcon(holder.mShareIconView, drawable, /* keepSpacing= */ false);
        } else if (propertyKey == END_BUTTON_CONTENT_DESC) {
            holder.mShareIconView.setContentDescription(
                    view.getContext()
                            .getString(
                                    R.string.accessibility_menu_share_via,
                                    model.get(END_BUTTON_CONTENT_DESC)));
        } else if (propertyKey == END_BUTTON_CLICK_LISTENER) {
            holder.mShareIconView.setOnClickListener(model.get(END_BUTTON_CLICK_LISTENER));
        }

        assert (holder.mStartIconView.getVisibility() != View.VISIBLE)
                        || (holder.mShareIconView.getVisibility() != View.VISIBLE)
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
