// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.NonNull;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/** Coordinator for the new tab page customization main bottom sheet. */
public class NtpCustomizationCoordinator {
    private final Context mContext;

    private final BottomSheetController mBottomSheetController;

    public NtpCustomizationCoordinator(
            @NonNull Context context, @NonNull BottomSheetController bottomSheetController) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
    }

    /** Open the new tab page customization main bottom sheet. */
    public void showBottomSheet() {
        View view =
                LayoutInflater.from(mContext)
                        .inflate(R.layout.ntp_customization_main_bottom_sheet, /* root= */ null);

        mBottomSheetController.requestShowContent(
                new NtpCustomizationMainBottomSheetContent(view), /* animate= */ true);
    }
}
