// Copyright 2020 The Chromium Authors
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

/** A custom binder used to bind the divider line in app menu. */
public class DividerLineMenuItemViewBinder implements CustomViewBinder {
    private static final int DIVIDER_LINE_ITEM_VIEW_TYPE = 0;

    @Override
    public int getViewTypeCount() {
        return 1;
    }

    @Override
    public int getItemViewType(int id) {
        return (id == R.id.divider_line_id
                        || id == R.id.managed_by_divider_line_id
                        || id == R.id.quick_delete_divider_line_id)
                ? DIVIDER_LINE_ITEM_VIEW_TYPE
                : CustomViewBinder.NOT_HANDLED;
    }

    @Override
    public int getLayoutId(int viewType) {
        if (viewType == DIVIDER_LINE_ITEM_VIEW_TYPE) {
            return R.layout.divider_line_menu_item;
        }
        return CustomViewBinder.NOT_HANDLED;
    }

    // TODO(crbug.com/40171104): create a PropertyModel only for divider line.
    @Override
    public void bind(PropertyModel model, View view, PropertyKey key) {
        AppMenuUtil.bindStandardItemEnterAnimation(model, view, key);

        if (key == AppMenuItemProperties.MENU_ITEM_ID) {
            int id = model.get(AppMenuItemProperties.MENU_ITEM_ID);
            assert id == R.id.divider_line_id
                    || id == R.id.managed_by_divider_line_id
                    || id == R.id.quick_delete_divider_line_id;
            view.setId(id);
            view.setEnabled(false);
        }
    }

    @Override
    public boolean supportsEnterAnimation(int id) {
        return true;
    }

    @Override
    public int getPixelHeight(Context context) {
        int dividerLineHeight =
                context.getResources().getDimensionPixelSize(R.dimen.divider_height);
        int paddingSize =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.overflow_menu_divider_line_padding);
        return dividerLineHeight + paddingSize * 2 /* top padding and bottom padding */;
    }
}
