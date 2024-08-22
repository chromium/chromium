// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.access_loss;

import android.content.Context;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;

class PasswordAccessLossWarningBridge {
    PasswordAccessLossWarningHelper mHelper;

    public PasswordAccessLossWarningBridge(
            Context context, BottomSheetController bottomSheetController) {
        mHelper = new PasswordAccessLossWarningHelper(context, bottomSheetController);
    }

    @CalledByNative
    @Nullable
    static PasswordAccessLossWarningBridge create(WindowAndroid windowAndroid) {
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
        mHelper.show(warningType);
    }
}
