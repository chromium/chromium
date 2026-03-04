// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetProperties.THIN_WEB_VIEW_HEIGHT;
import static org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetProperties.THIN_WEB_VIEW_INSET_BOTTOM;
import static org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetProperties.WEB_UI_CONTAINER_HEIGHT;

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
        if (THIN_WEB_VIEW_HEIGHT == propertyKey) {
            CoBrowseViews coBrowseViews = model.get(TabBottomSheetProperties.BOTTOM_SHEET_VIEWS);
            coBrowseViews.setThinWebViewHeight(model.get(THIN_WEB_VIEW_HEIGHT));
        } else if (WEB_UI_CONTAINER_HEIGHT == propertyKey) {
            CoBrowseViews coBrowseViews = model.get(TabBottomSheetProperties.BOTTOM_SHEET_VIEWS);
            coBrowseViews.setWebUiContainerHeight(model.get(WEB_UI_CONTAINER_HEIGHT));
        } else if (THIN_WEB_VIEW_INSET_BOTTOM == propertyKey) {
            CoBrowseViews coBrowseViews = model.get(TabBottomSheetProperties.BOTTOM_SHEET_VIEWS);
            coBrowseViews.setThinWebViewInsets(
                    /* top= */ 0,
                    /* left= */ 0,
                    model.get(THIN_WEB_VIEW_INSET_BOTTOM),
                    /* right= */ 0);
        }
    }
}
