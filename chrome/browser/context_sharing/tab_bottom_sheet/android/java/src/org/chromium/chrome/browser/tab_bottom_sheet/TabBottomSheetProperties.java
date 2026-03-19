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
    public static final WritableObjectPropertyKey<Integer> SHEET_HEIGHT =
            new WritableObjectPropertyKey<>("sheet_height");

    public static final PropertyKey[] ALL_KEYS = {BOTTOM_SHEET_VIEWS, SHEET_HEIGHT};

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
