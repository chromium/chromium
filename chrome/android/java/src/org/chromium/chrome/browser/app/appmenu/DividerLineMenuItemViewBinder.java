// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.appmenu;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.appmenu.AppMenuClickHandler;
import org.chromium.chrome.browser.ui.appmenu.CustomViewBinder;

/**
 * A custom binder used to bind the managed menu item.
 */
class DividerLineMenuItemViewBinder implements CustomViewBinder {
    private static final int DIVIDER_LINE_ITEM_VIEW_TYPE = 0;

    @Override
    public int getViewTypeCount() {
        return 1;
    }

    @Override
    public int getItemViewType(int id) {
        return (id == R.id.divider_line_id) ? DIVIDER_LINE_ITEM_VIEW_TYPE
                                            : CustomViewBinder.NOT_HANDLED;
    }

    @Override
    public View getView(MenuItem item, @Nullable View convertView, ViewGroup parent,
            LayoutInflater inflater, AppMenuClickHandler appMenuClickHandler,
            @Nullable Integer highlightedItemId) {
        assert item.getItemId() == R.id.divider_line_id;

        if (convertView == null) {
            convertView = inflater.inflate(R.layout.divider_line_menu_item, parent, false);
        }
        return convertView;
    }

    @Override
    public boolean supportsEnterAnimation(int id) {
        return true;
    }

    @Override
    public int getPixelHeight(Context context) {
        int dividerLineHeight =
                context.getResources().getDimensionPixelSize(R.dimen.divider_height);
        int paddingSize = context.getResources().getDimensionPixelSize(
                R.dimen.overflow_menu_divider_line_padding);
        return dividerLineHeight + paddingSize * 2 /* top padding and bottom padding */;
    }
}
