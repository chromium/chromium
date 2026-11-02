// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.email_verification;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniRawPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.autofill.anchored_dialog.AnchoredDialogCoordinator;
import org.chromium.chrome.browser.autofill.anchored_dialog.AnchoredDialogCoordinatorProvider;
import org.chromium.components.autofill.EmailVerificationPermissionUiStatus;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;

/**
 * Bridge class providing an entry point for AutofillClient to trigger the email verification bottom
 * sheet.
 */
@JNINamespace("autofill")
@NullMarked
public class EmailVerificationBottomSheetBridge
        implements EmailVerificationBottomSheetCoordinator.Delegate {
    @JniType("::autofill::EmailVerificationBottomSheetBridge")
    interface NativeEmailVerificationBottomSheetBridge extends JniTypeToken {}

    private @Nullable JniRawPtr<NativeEmailVerificationBottomSheetBridge> mNativeBridge;
    private final @Nullable Context mContext;
    private final @Nullable BottomSheetController mBottomSheetController;
    private final @Nullable AnchoredDialogCoordinator mAnchoredDialogCoordinator;
    private @Nullable EmailVerificationBottomSheetCoordinator mCoordinator;

    @CalledByNative
    @VisibleForTesting
    /*package*/ EmailVerificationBottomSheetBridge(
            JniRawPtr<NativeEmailVerificationBottomSheetBridge> nativeBridge,
            WindowAndroid window) {
        mNativeBridge = nativeBridge;
        mContext = window.getContext().get();
        mBottomSheetController = BottomSheetControllerProvider.from(window);
        mAnchoredDialogCoordinator = AnchoredDialogCoordinatorProvider.from(window);
    }

    /**
     * Requests to show the bottom sheet.
     *
     * @param title The title of the prompt.
     * @param description The formatted body description of the prompt.
     */
    @CalledByNative
    public void requestShowContent(
            @JniType("std::u16string") String title,
            @JniType("std::u16string") String description) {
        if (mNativeBridge == null) {
            return;
        }
        if (mContext == null
                || mBottomSheetController == null
                || mAnchoredDialogCoordinator == null) {
            onUiDismissed(EmailVerificationPermissionUiStatus.OTHER);
            return;
        }

        if (mCoordinator != null) {
            // A bottom sheet is already showing. Do not replace it or disrupt an
            // active loading state.
            return;
        }

        mCoordinator =
                new EmailVerificationBottomSheetCoordinator(
                        mContext,
                        title,
                        description,
                        mBottomSheetController,
                        mAnchoredDialogCoordinator,
                        /* delegate= */ this);
        mCoordinator.requestShowContent();
    }

    /** Requests to hide the bottom sheet if showing. */
    @CalledByNative
    public void hide() {
        if (mNativeBridge == null || mCoordinator == null) return;
        mCoordinator.hide();
    }

    /** Called when the bottom sheet has been shown. */
    @Override
    public void onUiShown() {
        if (mNativeBridge == null) return;
        EmailVerificationBottomSheetBridgeJni.get().onUiShown(mNativeBridge);
    }

    /** Called when the confirm/verify button has been clicked. */
    @Override
    public void onUiAccepted() {
        if (mNativeBridge == null) return;
        EmailVerificationBottomSheetBridgeJni.get().onUiAccepted(mNativeBridge);
    }

    /** Called when the bottom sheet has been dismissed/hidden. */
    @Override
    public void onUiDismissed(@EmailVerificationPermissionUiStatus int reason) {
        mCoordinator = null;
        if (mNativeBridge == null) return;
        EmailVerificationBottomSheetBridgeJni.get().onUiDismissed(mNativeBridge, reason);
    }

    @CalledByNative
    @VisibleForTesting
    /*package*/ void destroy() {
        if (mNativeBridge != null) {
            mNativeBridge.release();
            mNativeBridge = null;
        }
        if (mCoordinator == null) return;
        mCoordinator.destroy();
        mCoordinator = null;
    }

    /*package*/ @Nullable EmailVerificationBottomSheetCoordinator getCoordinatorForTesting() {
        return mCoordinator;
    }

    @NativeMethods
    interface Natives {
        void onUiShown(JniPtr<NativeEmailVerificationBottomSheetBridge> self);

        void onUiAccepted(JniPtr<NativeEmailVerificationBottomSheetBridge> self);

        void onUiDismissed(
                JniPtr<NativeEmailVerificationBottomSheetBridge> self,
                @EmailVerificationPermissionUiStatus int reason);
    }
}
