// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.autofill_ai;

import android.content.Context;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniRawPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;

/** JNI bridge entry point to show the Autofill AI attribution verification bottom sheet. */
@JNINamespace("autofill")
@NullMarked
public class AutofillAiAttributionVerificationSheetBridge
        implements AutofillAiAttributionVerificationSheetCoordinator.Delegate {
    @JniType("::autofill::AutofillAiAttributionVerificationSheetBridge")
    interface NativeAutofillAiAttributionVerificationSheetBridge extends JniTypeToken {}

    private @Nullable JniRawPtr<NativeAutofillAiAttributionVerificationSheetBridge> mNativeBridge;
    private final WindowAndroid mWindowAndroid;
    private @Nullable AutofillAiAttributionVerificationSheetCoordinator mCoordinator;

    @CalledByNative
    public AutofillAiAttributionVerificationSheetBridge(
            JniRawPtr<NativeAutofillAiAttributionVerificationSheetBridge> nativeBridge,
            @JniType("ui::WindowAndroid*") WindowAndroid windowAndroid) {
        mNativeBridge = nativeBridge;
        mWindowAndroid = windowAndroid;
    }

    @CalledByNative
    public void requestShowContent() {
        if (mNativeBridge == null) {
            return;
        }
        if (mCoordinator != null) {
            mCoordinator.destroy();
            mCoordinator = null;
        }

        Context context = mWindowAndroid.getContext().get();
        BottomSheetController bottomSheetController =
                BottomSheetControllerProvider.from(mWindowAndroid);
        if (context == null || bottomSheetController == null) {
            onDismissed();
            return;
        }

        mCoordinator =
                new AutofillAiAttributionVerificationSheetCoordinator(
                        context, bottomSheetController, /* delegate= */ this);
        mCoordinator.requestShowContent();
    }

    @Override
    public void onDismissed() {
        mCoordinator = null;
        if (mNativeBridge == null) {
            return;
        }
        AutofillAiAttributionVerificationSheetBridgeJni.get().onDismissed(mNativeBridge);
    }

    @CalledByNative
    public void destroy() {
        if (mNativeBridge != null) {
            mNativeBridge.release();
            mNativeBridge = null;
        }
        if (mCoordinator != null) {
            mCoordinator.destroy();
            mCoordinator = null;
        }
    }

    @Nullable AutofillAiAttributionVerificationSheetCoordinator getCoordinatorForTesting() {
        return mCoordinator;
    }

    @NativeMethods
    interface Natives {
        void onDismissed(JniPtr<NativeAutofillAiAttributionVerificationSheetBridge> self);
    }
}
