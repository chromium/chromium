// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.appmenu;

import android.content.Context;
import android.content.res.TypedArray;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.ColorRes;
import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.appmenu.AppMenuPropertiesDelegateImpl.ThreeButtonActionBarType;
import org.chromium.chrome.browser.ui.appmenu.AppMenuClickHandler;
import org.chromium.chrome.browser.ui.appmenu.CustomViewBinder;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter;
import org.chromium.components.browser_ui.widget.text.TextViewWithCompoundDrawables;
import org.chromium.ui.widget.ChipView;

/**
 * A custom view binder used to bind a menu item with an optional chip shown after the main menu
 * item text.
 */
class ChipViewMenuItemViewBinder implements CustomViewBinder {
    private static final int CHIP_VIEW_ITEM_VIEW_TYPE = 0;
    private final @ThreeButtonActionBarType int mThreeButtonActionBarType;

    ChipViewMenuItemViewBinder(@ThreeButtonActionBarType int threeButtonActionBarType) {
        mThreeButtonActionBarType = threeButtonActionBarType;
    }

    @Override
    public int getViewTypeCount() {
        return 1;
    }

    @Override
    public int getItemViewType(int id) {
        return (id == R.id.downloads_row_menu_id || id == R.id.all_bookmarks_row_menu_id)
                ? CHIP_VIEW_ITEM_VIEW_TYPE
                : CustomViewBinder.NOT_HANDLED;
    }

    @Override
    public View getView(MenuItem item, @Nullable View convertView, ViewGroup parent,
            LayoutInflater inflater, AppMenuClickHandler appMenuClickHandler,
            @Nullable Integer highlightedItemId) {
        assert item.getItemId() == R.id.downloads_row_menu_id
                || item.getItemId() == R.id.all_bookmarks_row_menu_id;

        final MenuItem destinationItem = item.getSubMenu().getItem(0);
        final MenuItem actionItem = item.getSubMenu().getItem(1);

        // By default, when no experiments are enabled, the menu item should go to the destination
        // (downloads manager or bookmarks manager).
        MenuItem mainMenuItem = destinationItem;
        MenuItem chipViewMenuItem = null;

        if (mThreeButtonActionBarType == ThreeButtonActionBarType.ACTION_CHIP_VIEW) {
            // If the action chip view variant is enabled, the chip view will be the add bookmark or
            // add download action.
            chipViewMenuItem = actionItem;
        } else if (mThreeButtonActionBarType == ThreeButtonActionBarType.DESTINATION_CHIP_VIEW) {
            // Else, if the destination chip view variant is enabled, the chip view will be the
            // destination and the menu item text will be the action.
            mainMenuItem = actionItem;
            chipViewMenuItem = destinationItem;
        } // else the menu item will not have chip view as other normal menu items.

        ChipViewMenuItemViewHolder holder;
        if (convertView == null || !(convertView.getTag() instanceof ChipViewMenuItemViewHolder)) {
            holder = new ChipViewMenuItemViewHolder();
            convertView = inflater.inflate(R.layout.chip_view_menu_item, parent, false);
            holder.title = convertView.findViewById(R.id.title);
            holder.chipView = convertView.findViewById(R.id.chip_view);
            convertView.setTag(holder);
        } else {
            holder = (ChipViewMenuItemViewHolder) convertView.getTag();
        }

        holder.title.setCompoundDrawablesRelative(mainMenuItem.getIcon(), null, null, null);
        holder.title.setText(mainMenuItem.getTitle());
        holder.title.setEnabled(mainMenuItem.isEnabled());
        final MenuItem finalMainMenuItem = mainMenuItem;
        holder.title.setOnClickListener(v -> appMenuClickHandler.onItemClick(finalMainMenuItem));
        @ColorRes
        int theme = mainMenuItem.isChecked() ? R.color.blue_mode_tint
                                             : R.color.default_icon_color_secondary_tint_list;
        holder.title.setDrawableTintColor(
                AppCompatResources.getColorStateList(convertView.getContext(), theme));

        if (chipViewMenuItem != null) {
            holder.chipView.setVisibility(View.VISIBLE);
            holder.chipView.getPrimaryTextView().setText(chipViewMenuItem.getTitle());
            holder.chipView.setEnabled(chipViewMenuItem.isEnabled());
            final MenuItem finalChipViewMenuItem = chipViewMenuItem;
            holder.chipView.setOnClickListener(
                    v -> appMenuClickHandler.onItemClick(finalChipViewMenuItem));

            if (highlightedItemId != null && chipViewMenuItem.getItemId() == highlightedItemId) {
                ViewHighlighter.turnOnRectangularHighlight(
                        holder.chipView, holder.chipView.getCornerRadius());
            } else {
                ViewHighlighter.turnOffHighlight(holder.chipView);
            }
        }

        if (highlightedItemId != null && mainMenuItem.getItemId() == highlightedItemId) {
            ViewHighlighter.turnOnRectangularHighlight(holder.title);
        } else {
            ViewHighlighter.turnOffHighlight(holder.title);
        }

        convertView.setEnabled(false);

        return convertView;
    }

    @Override
    public boolean supportsEnterAnimation(int id) {
        return true;
    }

    @Override
    public int getPixelHeight(Context context) {
        TypedArray a = context.obtainStyledAttributes(
                new int[] {android.R.attr.listPreferredItemHeightSmall});
        return a.getDimensionPixelSize(0, 0);
    }

    private static class ChipViewMenuItemViewHolder {
        public TextViewWithCompoundDrawables title;
        public ChipView chipView;
    }
}
