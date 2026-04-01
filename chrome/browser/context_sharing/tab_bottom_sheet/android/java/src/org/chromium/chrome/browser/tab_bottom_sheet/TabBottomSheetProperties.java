// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableFloatPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;

/** Properties for the "Tab Bottom Sheet" bottom sheet. */
@NullMarked
public class TabBottomSheetProperties {
    public static final ReadableObjectPropertyKey<CoBrowseViews> BOTTOM_SHEET_VIEWS =
            new ReadableObjectPropertyKey<>("bottom_sheet_views");
    public static final WritableIntPropertyKey SHEET_HEIGHT =
            new WritableIntPropertyKey("sheet_height");
    public static final WritableFloatPropertyKey PEEK_VIEW_AND_EXPANDED_CONTENT_ALPHA =
            new WritableFloatPropertyKey("peek_view_alpha_and_expanded_content_alpha");
    public static final WritableIntPropertyKey PEEK_VIEW_AND_EXPANDED_CONTENT_VISIBILITY =
            new WritableIntPropertyKey("peek_view_and_expanded_content_visibility");

    public static final PropertyKey[] ALL_KEYS = {
        BOTTOM_SHEET_VIEWS,
        SHEET_HEIGHT,
        PEEK_VIEW_AND_EXPANDED_CONTENT_ALPHA,
        PEEK_VIEW_AND_EXPANDED_CONTENT_VISIBILITY,
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
