// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.vcn;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/** Bridge for the virtual card enrollment bottom sheet. */
@JNINamespace("autofill")
/*package*/ class AutofillVcnEnrollBottomSheetBridge {
    private long mNativeAutofillVcnEnrollBottomSheetBridge;
    private AutofillVcnEnrollBottomSheetCoordinator mCoordinator;

    @CalledByNative
    @VisibleForTesting
    /*package*/ AutofillVcnEnrollBottomSheetBridge() {}

    @CalledByNative
    @VisibleForTesting
    /*package*/ boolean requestShowContent(long nativeAutofillVcnEnrollBottomSheetBridge,
            WebContents webContents, String messageText, String acceptButtonLabel,
            String cancelButtonLabel) {
        if (webContents == null || webContents.isDestroyed()) return false;

        WindowAndroid window = webContents.getTopLevelNativeWindow();
        if (window == null) return false;

        if (mNativeAutofillVcnEnrollBottomSheetBridge != 0) return false;
        mNativeAutofillVcnEnrollBottomSheetBridge = nativeAutofillVcnEnrollBottomSheetBridge;

        mCoordinator = new AutofillVcnEnrollBottomSheetCoordinator(window.getContext().get(),
                messageText, acceptButtonLabel, cancelButtonLabel, this::onAccept, this::onCancel,
                this::onDismiss);

        return mCoordinator.requestShowContent(window);
    }

    @VisibleForTesting
    /*package*/ void onAccept() {
        if (mNativeAutofillVcnEnrollBottomSheetBridge == 0) return;
        AutofillVcnEnrollBottomSheetBridgeJni.get().onAccept(
                mNativeAutofillVcnEnrollBottomSheetBridge);
        mNativeAutofillVcnEnrollBottomSheetBridge = 0;
    }

    @VisibleForTesting
    /*package*/ void onCancel() {
        if (mNativeAutofillVcnEnrollBottomSheetBridge == 0) return;
        AutofillVcnEnrollBottomSheetBridgeJni.get().onCancel(
                mNativeAutofillVcnEnrollBottomSheetBridge);
        mNativeAutofillVcnEnrollBottomSheetBridge = 0;
    }

    @VisibleForTesting
    /*package*/ void onDismiss() {
        if (mNativeAutofillVcnEnrollBottomSheetBridge == 0) return;
        AutofillVcnEnrollBottomSheetBridgeJni.get().onDismiss(
                mNativeAutofillVcnEnrollBottomSheetBridge);
        mNativeAutofillVcnEnrollBottomSheetBridge = 0;
    }

    @CalledByNative
    @VisibleForTesting
    /*package*/ void hide() {
        mNativeAutofillVcnEnrollBottomSheetBridge = 0;
        if (mCoordinator == null) return;
        mCoordinator.hide();
        mCoordinator = null;
    }

    @NativeMethods
    interface Natives {
        void onAccept(long nativeAutofillVCNEnrollBottomSheetBridge);
        void onCancel(long nativeAutofillVCNEnrollBottomSheetBridge);
        void onDismiss(long nativeAutofillVCNEnrollBottomSheetBridge);
    }
}
