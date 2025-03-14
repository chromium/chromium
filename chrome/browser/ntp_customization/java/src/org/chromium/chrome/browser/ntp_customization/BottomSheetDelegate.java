// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import android.view.View;

/**
 * This delegate interface is responsible for adding and displaying bottom sheet layouts, as well as
 * handling system back press and back button clicks on the bottom sheet.
 */
public interface BottomSheetDelegate {
    /**
     * Records the position of the bottom sheet layout in the view flipper view.
     *
     * @param type The type of the bottom sheet.
     */
    void registerBottomSheetLayout(
            @NtpCustomizationCoordinator.BottomSheetType int type, View view);

    /** Handles system back press and back button clicks on the bottom sheet. */
    void backPressOnCurrentBottomSheet();
}
