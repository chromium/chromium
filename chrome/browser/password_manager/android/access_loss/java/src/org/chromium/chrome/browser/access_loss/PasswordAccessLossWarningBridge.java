// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.access_loss;

import android.content.Context;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;

import org.chromium.chrome.browser.bottom_sheet.SimpleNoticeSheetCoordinator;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;

class PasswordAccessLossWarningBridge {
    final Context mContext;
    final BottomSheetController mBottomSheetController;

    public PasswordAccessLossWarningBridge(
            Context context, BottomSheetController bottomSheetController) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
    }

    @CalledByNative
    @Nullable
    private static PasswordAccessLossWarningBridge create(WindowAndroid windowAndroid) {
        BottomSheetController bottomSheetController =
                BottomSheetControllerProvider.from(windowAndroid);
        if (bottomSheetController == null) {
            return null;
        }
        Context context = windowAndroid.getContext().get();
        if (context == null) {
            return null;
        }
        return new PasswordAccessLossWarningBridge(context, bottomSheetController);
    }

    @CalledByNative
    public void show(@PasswordAccessLossWarningType int warningType) {
        SimpleNoticeSheetCoordinator coordinator =
                new SimpleNoticeSheetCoordinator(mContext, mBottomSheetController);
        // TODO: crbug.com/353283268 - Use the warningType to show the sheet with specific looks.
        coordinator.showSheet();
    }
}
