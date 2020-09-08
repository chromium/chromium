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

import org.chromium.chrome.R;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.ui.appmenu.CustomViewBinder;
import org.chromium.components.browser_ui.widget.text.TextViewWithCompoundDrawables;
import org.chromium.ui.widget.ChromeImageView;

/**
 * A custom binder used to bind the incognito menu item.
 */
class IncognitoMenuItemViewBinder implements CustomViewBinder {
    private static final int INCOGNITO_ITEM_VIEW_TYPE = 0;

    @Override
    public int getViewTypeCount() {
        return 1;
    }

    @Override
    public int getItemViewType(int id) {
        return id == R.id.new_incognito_tab_menu_id ? INCOGNITO_ITEM_VIEW_TYPE
                                                    : CustomViewBinder.NOT_HANDLED;
    }

    @Override
    public View getView(
            MenuItem item, View convertView, ViewGroup parent, LayoutInflater inflater) {
        assert item.getItemId() == R.id.new_incognito_tab_menu_id;

        IncognitoMenuItemViewHolder holder;
        if (convertView == null || !(convertView.getTag() instanceof IncognitoMenuItemViewHolder)) {
            holder = new IncognitoMenuItemViewHolder();
            convertView = inflater.inflate(R.layout.incognito_menu_item, parent, false);
            holder.title = convertView.findViewById(R.id.menu_item_text);
            holder.image = convertView.findViewById(R.id.management_icon);
            convertView.setTag(holder);
        } else {
            holder = (IncognitoMenuItemViewHolder) convertView.getTag();
        }

        holder.title.setCompoundDrawablesRelative(item.getIcon(), null, null, null);
        holder.title.setEnabled(item.isEnabled());
        holder.title.setFocusable(item.isEnabled());
        if (IncognitoUtils.isIncognitoModeManaged()) {
            holder.image.setVisibility(View.VISIBLE);
        }

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

    private static class IncognitoMenuItemViewHolder {
        public TextViewWithCompoundDrawables title;
        public ChromeImageView image;
    }
}
