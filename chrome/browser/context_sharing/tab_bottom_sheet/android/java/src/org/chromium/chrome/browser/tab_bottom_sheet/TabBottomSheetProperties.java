// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import androidx.annotation.Px;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableFloatPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for the Tab Bottom Sheet. */
@NullMarked
public class TabBottomSheetProperties {
    public static class ResizingState {
        public final @Px int webUiContainerHeight;
        public final float heightFraction;

        /**
         * @param webUiContainerHeight The height of the webUi container.
         * @param heightFraction The percentage of the bottom sheet visible in relation to the
         *     maximum height.
         */
        public ResizingState(@Px int webUiContainerHeight, float heightFraction) {
            this.webUiContainerHeight = webUiContainerHeight;
            this.heightFraction = heightFraction;
        }
    }

    public static final ReadableObjectPropertyKey<CoBrowseViews> BOTTOM_SHEET_VIEWS =
            new ReadableObjectPropertyKey<>("bottom_sheet_views");
    public static final WritableObjectPropertyKey<ResizingState> RESIZING_STATE =
            new WritableObjectPropertyKey<>("resizing_state");
    public static final WritableBooleanPropertyKey IS_RESIZING =
            new WritableBooleanPropertyKey("is_resizing");
    public static final WritableFloatPropertyKey PEEK_STATE_ALPHA =
            new WritableFloatPropertyKey("peek_state_alpha");

    public static final PropertyKey[] ALL_KEYS = {
        BOTTOM_SHEET_VIEWS, RESIZING_STATE, IS_RESIZING, PEEK_STATE_ALPHA
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
                .with(IS_RESIZING, false)
                .build();
    }
}
