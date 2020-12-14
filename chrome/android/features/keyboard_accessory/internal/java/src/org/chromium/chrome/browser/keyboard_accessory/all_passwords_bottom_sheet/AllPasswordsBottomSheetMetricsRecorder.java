// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;

/**
 * This class provides helpers to record metrics related to the AllPasswordsBottomSheet.
 */
class AllPasswordsBottomSheetMetricsRecorder {
    static final String UMA_ALL_PASSWORDS_BOTTOM_SHEET_ACTIONS =
            "PasswordManager.AllPasswordsBottomSheet.UserAction";

    // Used to record metrics for the AllPasswordsBottomSheet actions. Entries should
    // not be renumbered and numeric values should never be reused. Must be kept in
    // sync with the enum in enums.xml.
    @IntDef({AllPasswordsBottomSheetActions.CREDENTIAL_SELECTED,
            AllPasswordsBottomSheetActions.SHEET_DISMISSED,
            AllPasswordsBottomSheetActions.SEARCH_USED})
    @interface AllPasswordsBottomSheetActions {
        int CREDENTIAL_SELECTED = 0;
        int SHEET_DISMISSED = 1;
        int SEARCH_USED = 2;
        int COUNT = 3;
    }

    static void recordHistogram(String name, int action, int max) {
        RecordHistogram.recordEnumeratedHistogram(name, action, max);
    }
}
