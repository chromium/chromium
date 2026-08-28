// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.wallet_reminder_notice;

import android.content.Context;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.autofill.payments.LegalMessageLine;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;

import java.util.List;

/** JNI bridge entry point to show the Wallet Reminder Notice bottom sheet. */
@JNINamespace("autofill")
@NullMarked
public class AutofillWalletReminderNoticeBottomSheetBridge {
    private final WindowAndroid mWindowAndroid;
    private @Nullable AutofillWalletReminderNoticeBottomSheetCoordinator mCoordinator;

    @CalledByNative
    public AutofillWalletReminderNoticeBottomSheetBridge(WindowAndroid windowAndroid) {
        mWindowAndroid = windowAndroid;
    }

    @CalledByNative
    public void requestShowContent(
            @JniType("std::vector") List<LegalMessageLine> legalMessageLines) {
        Context context = mWindowAndroid.getContext().get();
        BottomSheetController bottomSheetController =
                BottomSheetControllerProvider.from(mWindowAndroid);
        if (context == null || bottomSheetController == null) {
            return;
        }

        if (mCoordinator != null) {
            mCoordinator.destroy();
        }

        mCoordinator =
                new AutofillWalletReminderNoticeBottomSheetCoordinator(
                        context, bottomSheetController, legalMessageLines);
        mCoordinator.requestShowContent();
    }

    @CalledByNative
    public void destroy() {
        if (mCoordinator != null) {
            mCoordinator.destroy();
            mCoordinator = null;
        }
    }

    @Nullable AutofillWalletReminderNoticeBottomSheetCoordinator getCoordinatorForTesting() {
        return mCoordinator;
    }
}
