// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.tab_bottom_sheet;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Utility methods used by the Tab Bottom Sheet components. */
@NullMarked
public final class TabBottomSheetUtils {
    private TabBottomSheetUtils() {}

    public static boolean isTabBottomSheetEnabled() {
        return ChromeFeatureList.sTabBottomSheet.isEnabled();
    }

    public static boolean shouldShowFusebox() {
        return !ChromeFeatureList.sTabBottomSheetDontShowFusebox.getValue();
    }
}
