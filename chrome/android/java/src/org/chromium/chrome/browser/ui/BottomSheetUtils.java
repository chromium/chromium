// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/** Utility methods for the Bottom Sheet. */
@NullMarked
public final class BottomSheetUtils {
    private BottomSheetUtils() {}

    /**
     * Returns whether the bottom sheet controller holds content that acts as browser controls,
     * checking the feature flag and whether it is enabled for the current context as well.
     *
     * @param controller The {@link BottomSheetController} to check.
     * @param isBottomSheetAsBrowserControlsEnabled Whether bottom sheet acting as browser controls
     *     is enabled for the current context.
     */
    public static boolean isContentActingAsBrowserControls(
            @Nullable BottomSheetController controller,
            boolean isBottomSheetAsBrowserControlsEnabled) {
        if (controller == null) return false;
        if (!isBottomSheetAsBrowserControlsEnabled) return false;

        BottomSheetContent content = controller.getCurrentSheetContent();
        return content != null && content.actsAsBrowserControls();
    }
}
