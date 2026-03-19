// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** ViewBinder for the Tab Bottom Sheet. */
@NullMarked
public class TabBottomSheetViewBinder {
    /**
     * Binds the given model to the given view.
     *
     * @param model The {@link PropertyModel} to bind.
     * @param view The inflated Android {@link View} of the promo sheet.
     * @param propertyKey The {@link PropertyKey} that changed.
     */
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == TabBottomSheetProperties.SHEET_HEIGHT) {
            int sheetHeight = model.get(TabBottomSheetProperties.SHEET_HEIGHT);
            CoBrowseViews coBrowseViews = model.get(TabBottomSheetProperties.BOTTOM_SHEET_VIEWS);
            coBrowseViews.setSheetHeight(sheetHeight);
        }
    }
}
