// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for the "Tab Bottom Sheet" bottom sheet. */
@NullMarked
public class TabBottomSheetProperties {
    public static final ReadableObjectPropertyKey<CoBrowseViews> BOTTOM_SHEET_VIEWS =
            new ReadableObjectPropertyKey<>("bottom_sheet_views");
    public static final WritableObjectPropertyKey<Integer> THIN_WEB_VIEW_HEIGHT =
            new WritableObjectPropertyKey<>("thin_web_view_height");
    public static final WritableObjectPropertyKey<Integer> WEB_UI_CONTAINER_HEIGHT =
            new WritableObjectPropertyKey<>("web_ui_container_height");
    public static final WritableObjectPropertyKey<Integer> THIN_WEB_VIEW_INSET_BOTTOM =
            new WritableObjectPropertyKey<>("thin_web_view_inset_bottom");

    public static final PropertyKey[] ALL_KEYS = {
        BOTTOM_SHEET_VIEWS,
        THIN_WEB_VIEW_HEIGHT,
        WEB_UI_CONTAINER_HEIGHT,
        THIN_WEB_VIEW_INSET_BOTTOM
    };

    /**
     * Creates a default model structure. Listeners will be populated by the Coordinator.
     *
     * @param coBrowseViews The views to show in the bottom sheet.
     * @return A new {@link PropertyModel} instance.
     */
    public static PropertyModel createDefaultModel(CoBrowseViews coBrowseViews) {
        return new PropertyModel.Builder(ALL_KEYS).with(BOTTOM_SHEET_VIEWS, coBrowseViews).build();
    }
}
