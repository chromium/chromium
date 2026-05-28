// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_tasks;

import android.view.View;

import androidx.annotation.ColorInt;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.GlowSpec;

/** Concrete implementation of {@link TabBottomSheetContent} for the Contextual Tasks feature. */
@NullMarked
public class ContextualTaskBottomSheetContent extends TabBottomSheetContent {
    /**
     * Constructor.
     *
     * @param contentView The inflated view for the bottom sheet.
     * @param fullHeightRatio The full height ratio for the bottom sheet.
     * @param backgroundColor The background color for the bottom sheet.
     */
    public ContextualTaskBottomSheetContent(
            View contentView, float fullHeightRatio, @ColorInt int backgroundColor) {
        super(contentView, fullHeightRatio, backgroundColor);
    }

    @Override
    public @Nullable GlowSpec getSheetBackgroundGlowSpecOverride() {
        return null;
    }
}
