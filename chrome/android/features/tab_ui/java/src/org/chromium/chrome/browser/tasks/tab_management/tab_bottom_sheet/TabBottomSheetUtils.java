// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.tab_bottom_sheet;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Utility methods used by the Tab Bottom Sheet components. */
@NullMarked
public final class TabBottomSheetUtils {
    private TabBottomSheetUtils() {}

    /** Modes for the Tab Bottom Sheet. */
    @IntDef({TabBottomSheetModes.SIMPLE, TabBottomSheetModes.COMPLEX})
    @Retention(RetentionPolicy.SOURCE)
    @interface TabBottomSheetModes {
        int SIMPLE = 0;
        int COMPLEX = 1;
    }
}
