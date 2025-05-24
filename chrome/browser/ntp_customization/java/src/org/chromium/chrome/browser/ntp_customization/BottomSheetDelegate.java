// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import android.view.View;

import org.chromium.build.annotations.NullMarked;

/**
 * This delegate interface is responsible for recording the position of the bottom sheet layout in
 * the view flipper view, as well as handling back button clicks on the bottom sheet.
 */
@NullMarked
public interface BottomSheetDelegate {
    /**
     * Records the position of the bottom sheet layout in the view flipper view. The position index
     * starts at 0.
     *
     * @param type The type of the bottom sheet.
     */
    void registerBottomSheetLayout(
            @NtpCustomizationCoordinator.BottomSheetType int type, View view);

    /** Handles back button clicks in the bottom sheet. */
    void backPressOnCurrentBottomSheet();

    /**
     * Determines whether a bottom sheet should be displayed in a standalone mode, isolated from the
     * navigation flow staring from the main bottom sheet.
     *
     * @return True if the bottom sheet should be shown by itself (i.e., without the main bottom
     *     sheet); False if it should be part of the full navigation flow starting from the main
     *     bottom sheet.
     */
    boolean shouldShowAlone();
}
