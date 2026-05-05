// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;


import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableFloatPropertyKey;

/** Properties for the Tab Bottom Sheet. */
@NullMarked
public class TabBottomSheetProperties {


    public static final ReadableObjectPropertyKey<CoBrowseViews> BOTTOM_SHEET_VIEWS =
            new ReadableObjectPropertyKey<>("bottom_sheet_views");
    public static final ReadableObjectPropertyKey<WebViewResizingHelper> WEB_VIEW_RESIZING_HELPER =
            new ReadableObjectPropertyKey<>("web_view_resizing_helper");

    public static final WritableFloatPropertyKey PEEK_STATE_ALPHA =
            new WritableFloatPropertyKey("peek_state_alpha");
    public static final WritableFloatPropertyKey EXPANDED_STATE_ALPHA =
            new WritableFloatPropertyKey("expanded_state_alpha");

    public static final PropertyKey[] ALL_KEYS = {
        BOTTOM_SHEET_VIEWS, WEB_VIEW_RESIZING_HELPER, PEEK_STATE_ALPHA, EXPANDED_STATE_ALPHA
    };

    /**
     * Creates a default model structure. Listeners will be populated by the Coordinator.
     *
     * @param coBrowseViews The views to show in the bottom sheet.
     * @return A new {@link PropertyModel} instance.
     */
    public static PropertyModel createDefaultModel(CoBrowseViews coBrowseViews) {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(BOTTOM_SHEET_VIEWS, coBrowseViews)
                .with(WEB_VIEW_RESIZING_HELPER, coBrowseViews.getWebViewResizingHelper())
                .build();
    }
}
