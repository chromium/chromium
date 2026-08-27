// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.content.Context;
import android.content.pm.ResolveInfo;
import android.os.Build;
import android.view.MenuItem;
import android.webkit.SelectionActionMenuClient;

import androidx.annotation.RequiresApi;

import org.chromium.base.SelectionActionMenuClientWrapper;
import org.chromium.base.SelectionActionMenuClientWrapper.DefaultItem;
import org.chromium.base.SelectionActionMenuClientWrapper.MenuType;
import org.chromium.build.annotations.NullMarked;

import java.util.List;

@RequiresApi(Build.VERSION_CODES.CINNAMON_BUN)
@NullMarked
/* package */ class SelectionActionMenuClientAdapter implements SelectionActionMenuClientWrapper {
    private final SelectionActionMenuClient mClient;

    /* package */ SelectionActionMenuClientAdapter(SelectionActionMenuClient client) {
        mClient = client;
    }

    @Override
    public @DefaultItem int[] getDefaultMenuItemOrder(@MenuType int menuType) {
        return mClient.getDefaultMenuItemOrder(menuType);
    }

    @Override
    public List<MenuItem> getAdditionalMenuItems(
            Context context,
            @MenuType int menuType,
            boolean isSelectionPassword,
            boolean isSelectionReadOnly,
            String selectedText) {
        return mClient.getAdditionalMenuItems(
                context, menuType, isSelectionPassword, isSelectionReadOnly, selectedText);
    }

    @Override
    public List<ResolveInfo> filterTextProcessingActivities(
            Context context, @MenuType int menuType, List<ResolveInfo> activities) {
        return mClient.filterTextProcessingActivities(context, menuType, activities);
    }

    @Override
    public boolean handleMenuItemClick(Context context, MenuItem item) {
        return mClient.handleMenuItemClick(context, item);
    }

    // Ensure that all the constants map correctly to the platform values as we're sending them
    // through to upstream code without re-mapping.
    static {
        assert DefaultItem.CUT == SelectionActionMenuClient.DEFAULT_ITEM_CUT;
        assert DefaultItem.COPY == SelectionActionMenuClient.DEFAULT_ITEM_COPY;
        assert DefaultItem.PASTE == SelectionActionMenuClient.DEFAULT_ITEM_PASTE;
        assert DefaultItem.PASTE_AS_PLAIN_TEXT
                == SelectionActionMenuClient.DEFAULT_ITEM_PASTE_AS_PLAIN_TEXT;
        assert DefaultItem.SHARE == SelectionActionMenuClient.DEFAULT_ITEM_SHARE;
        assert DefaultItem.SELECT_ALL == SelectionActionMenuClient.DEFAULT_ITEM_SELECT_ALL;
        assert DefaultItem.WEB_SEARCH == SelectionActionMenuClient.DEFAULT_ITEM_WEB_SEARCH;

        assert MenuType.FLOATING == SelectionActionMenuClient.MENU_TYPE_FLOATING;
        assert MenuType.DROPDOWN == SelectionActionMenuClient.MENU_TYPE_DROPDOWN;
    }
}
