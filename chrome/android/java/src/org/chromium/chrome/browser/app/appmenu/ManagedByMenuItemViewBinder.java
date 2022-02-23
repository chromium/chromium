// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.appmenu;

import android.content.Context;
import android.view.View;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties;
import org.chromium.chrome.browser.ui.appmenu.AppMenuUtil;
import org.chromium.chrome.browser.ui.appmenu.CustomViewBinder;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** A custom binder used to bind the managed by menu item. */
class ManagedByMenuItemViewBinder implements CustomViewBinder {
    private static final int MANAGED_BY_ITEM_VIEW_TYPE = 0;

    @Override
    public int getViewTypeCount() {
        return 1;
    }

    @Override
    public int getItemViewType(int id) {
        return id == R.id.managed_by_menu_id ? MANAGED_BY_ITEM_VIEW_TYPE
                                             : CustomViewBinder.NOT_HANDLED;
    }

    @Override
    public int getLayoutId(int viewType) {
        if (viewType == MANAGED_BY_ITEM_VIEW_TYPE) {
            return R.layout.managed_by_menu_item;
        }
        return CustomViewBinder.NOT_HANDLED;
    }

    @Override
    public void bind(PropertyModel model, View view, PropertyKey key) {
        AppMenuUtil.bindStandardItemEnterAnimation(model, view, key);

        if (key == AppMenuItemProperties.MENU_ITEM_ID) {
            int id = model.get(AppMenuItemProperties.MENU_ITEM_ID);
            assert id == R.id.managed_by_menu_id;

            view.setId(id);
        }
    }

    @Override
    public boolean supportsEnterAnimation(int id) {
        return true;
    }

    @Override
    public int getPixelHeight(Context context) {
        // TODO(crbug.com/1124607): Update this menu item for new app menu.
        int dividerLineHeight =
                context.getResources().getDimensionPixelSize(R.dimen.divider_height);
        int itemSize = context.getResources().getDimensionPixelSize(
                R.dimen.overflow_menu_managed_by_min_height);
        return dividerLineHeight + itemSize;
    }
}
