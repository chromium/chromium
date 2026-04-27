// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/** Utility methods for the Bottom Sheet. */
@NullMarked
public final class BottomSheetUtils {
    private BottomSheetUtils() {}

    /**
     * Returns whether the bottom sheet controller holds content that acts as browser controls,
     * checking the feature flag as well.
     */
    public static boolean isContentActingAsBrowserControls(
            @Nullable BottomSheetController controller) {
        if (controller == null) return false;
        if (!ChromeFeatureList.sBottomSheetAsBrowserControls.isEnabled()) return false;

        BottomSheetContent content = controller.getCurrentSheetContent();
        return content != null && content.actsAsBrowserControls();
    }
}
