// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.view.View;

import androidx.annotation.ColorInt;

import org.chromium.build.annotations.NullMarked;

/**
 * Interface providing specialized implementations of {@link TabBottomSheetContent} for different
 * client features.
 */
@NullMarked
public interface TabBottomSheetContentProvider {
    /**
     * Instantiates a new instance of {@link TabBottomSheetContent}.
     *
     * @param contentView The content view shown inside the bottom sheet.
     * @param fullHeightRatio The target height ratio of the sheet in full state.
     * @param backgroundColor The background color of the sheet.
     * @return A non-null custom or default {@link TabBottomSheetContent}.
     */
    TabBottomSheetContent create(
            View contentView, float fullHeightRatio, @ColorInt int backgroundColor);
}
