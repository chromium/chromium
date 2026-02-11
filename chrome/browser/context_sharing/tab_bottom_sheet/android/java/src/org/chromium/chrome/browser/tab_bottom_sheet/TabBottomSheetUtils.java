// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.base.WindowAndroid;

/** Utility methods used by the Tab Bottom Sheet components. */
@NullMarked
public final class TabBottomSheetUtils {
    private static final org.chromium.base.UnownedUserDataKey<TabBottomSheetManager> KEY =
            new org.chromium.base.UnownedUserDataKey<>();

    private TabBottomSheetUtils() {}

    public static boolean isTabBottomSheetEnabled() {
        return ChromeFeatureList.sTabBottomSheet.isEnabled();
    }

    public static boolean shouldShowFusebox() {
        return !ChromeFeatureList.sTabBottomSheetDontShowFusebox.getValue();
    }

    // Attach TabBottomSheetManager to WindowAndroid.
    // This allows TabBottomSheetManager to be retrieved statically.
    static void attachManagerToWindow(WindowAndroid windowAndroid, TabBottomSheetManager manager) {
        KEY.attachToHost(windowAndroid.getUnownedUserDataHost(), manager);
    }

    // Detach TabBottomSheetManager from WindowAndroid.
    static void detachManagerFromWindow(WindowAndroid windowAndroid) {
        KEY.detachFromHost(windowAndroid.getUnownedUserDataHost());
    }

    static @Nullable TabBottomSheetManager getManagerFromWindow(WindowAndroid windowAndroid) {
        return KEY.retrieveDataFromHost(windowAndroid.getUnownedUserDataHost());
    }
}
