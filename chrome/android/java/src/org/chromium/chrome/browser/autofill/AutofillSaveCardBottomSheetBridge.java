// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.net.Uri;

import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.autofill.payments.AutofillSaveCardUiInfo;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.base.WindowAndroid;

import java.util.function.Consumer;

/**
 * Bridge class providing an entry point for autofill client to trigger the save card bottom sheet.
 */
@JNINamespace("autofill")
public class AutofillSaveCardBottomSheetBridge
        extends EmptyBottomSheetObserver implements AutofillSaveCardBottomSheetContent.Delegate {
    private long mNativeAutofillSaveCardBottomSheetBridge;
    private WindowAndroid mWindow;
    private BottomSheetController mBottomSheetController;
    private AutofillSaveCardBottomSheetContent mBottomSheetContent;

    @CalledByNative
    @VisibleForTesting
    /* package */ AutofillSaveCardBottomSheetBridge(
            long nativeAutofillSaveCardBottomSheetBridge, WindowAndroid window) {
        mNativeAutofillSaveCardBottomSheetBridge = nativeAutofillSaveCardBottomSheetBridge;
        mWindow = window;
        mBottomSheetController = BottomSheetControllerProvider.from(window);
        mBottomSheetController.addObserver(this);
    }

    /**
     * Requests to show the bottom sheet.
     *
     * The bottom sheet may not be shown in some cases.
     * {@see BottomSheetController#requestShowContent}
     *
     * @param uiInfo An object providing text and images to the bottom sheet
     * view.
     */
    @CalledByNative
    public void requestShowContent(AutofillSaveCardUiInfo uiInfo) {
        if (mNativeAutofillSaveCardBottomSheetBridge != 0) {
            mBottomSheetContent = new AutofillSaveCardBottomSheetContent(
                    mWindow.getContext().get(), /*delegate=*/this);
            mBottomSheetContent.setUiInfo(uiInfo);
            if (mBottomSheetController.requestShowContent(mBottomSheetContent, /*animate=*/true)) {
                AutofillSaveCardBottomSheetBridgeJni.get().onUiShown(
                        mNativeAutofillSaveCardBottomSheetBridge);
            } else {
                mBottomSheetContent = null;
            }
        }
    }

    // AutofillSaveCardBottomSheetContent.Delegate implementation follows:
    @Override
    public void didClickLegalMessageUrl(String url) {
        new CustomTabsIntent.Builder().setShowTitle(true).build().launchUrl(
                mBottomSheetContent.getContentView().getContext(), Uri.parse(url));
    }

    @Override
    public void onSheetClosed(@StateChangeReason int reason) {
        switch (reason) {
            case StateChangeReason.BACK_PRESS: // Intentionally fall through.
            case StateChangeReason.SWIPE: // Intentionally fall through.
            case StateChangeReason.TAP_SCRIM:
                callBridgeAndReset(
                        (nativePointer)
                                -> AutofillSaveCardBottomSheetBridgeJni.get().onUiCanceled(
                                        nativePointer));
                break;
            case StateChangeReason.INTERACTION_COMPLETE:
                // Expecting didClickConfirm() and didClickCancel() call the delegate in this case.
                break;
            default:
                callBridgeAndReset(
                        (nativePointer)
                                -> AutofillSaveCardBottomSheetBridgeJni.get().onUiIgnored(
                                        nativePointer));
                break;
        }
    }

    @Override
    public void didClickConfirm() {
        mBottomSheetController.hideContent(
                mBottomSheetContent, /*animate=*/true, StateChangeReason.INTERACTION_COMPLETE);
        callBridgeAndReset(
                (nativePointer)
                        -> AutofillSaveCardBottomSheetBridgeJni.get().onUiAccepted(nativePointer));
    }

    @Override
    public void didClickCancel() {
        mBottomSheetController.hideContent(
                mBottomSheetContent, /*animate=*/true, StateChangeReason.INTERACTION_COMPLETE);
        callBridgeAndReset(
                (nativePointer)
                        -> AutofillSaveCardBottomSheetBridgeJni.get().onUiCanceled(nativePointer));
    }

    private void callBridgeAndReset(Consumer<Long> nativeMethod) {
        if (mBottomSheetContent != null && mNativeAutofillSaveCardBottomSheetBridge != 0) {
            nativeMethod.accept(mNativeAutofillSaveCardBottomSheetBridge);
        }
        mBottomSheetContent = null;
    }

    @CalledByNative
    @VisibleForTesting
    /*package*/ void destroy() {
        mBottomSheetController.removeObserver(this);
        callBridgeAndReset(
                (nativePointer)
                        -> AutofillSaveCardBottomSheetBridgeJni.get().onUiIgnored(nativePointer));
        mNativeAutofillSaveCardBottomSheetBridge = 0;
    }

    @NativeMethods
    public interface Natives {
        void onUiShown(long nativeAutofillSaveCardBottomSheetBridge);
        void onUiAccepted(long nativeAutofillSaveCardBottomSheetBridge);
        void onUiCanceled(long nativeAutofillSaveCardBottomSheetBridge);
        void onUiIgnored(long nativeAutofillSaveCardBottomSheetBridge);
    }
}
