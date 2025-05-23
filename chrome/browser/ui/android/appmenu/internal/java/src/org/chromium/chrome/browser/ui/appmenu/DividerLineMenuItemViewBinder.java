// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import android.content.Context;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.appmenu.internal.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** A custom binder used to bind the divider line in app menu. */
@NullMarked
public class DividerLineMenuItemViewBinder {
    /** Handles binding the view and models changes. */
    public static void bind(PropertyModel model, View view, PropertyKey key) {
        AppMenuUtil.bindStandardItemEnterAnimation(model, view, key);

        if (key == AppMenuItemProperties.MENU_ITEM_ID) {
            int id = model.get(AppMenuItemProperties.MENU_ITEM_ID);
            view.setId(id);
            view.setEnabled(false);
        }
    }

    /** Provides the minimum height for the view for menu sizing. */
    public static int getPixelHeight(Context context) {
        int dividerLineHeight =
                context.getResources().getDimensionPixelSize(R.dimen.divider_height);
        int paddingSize =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.overflow_menu_divider_line_padding);
        return dividerLineHeight + paddingSize * 2 /* top padding and bottom padding */;
    }
}
