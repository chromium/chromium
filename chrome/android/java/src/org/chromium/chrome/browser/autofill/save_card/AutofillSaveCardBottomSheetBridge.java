// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.save_card;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.layouts.LayoutManagerProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.autofill.payments.AutofillSaveCardUiInfo;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;

/**
 * Bridge class providing an entry point for autofill client to trigger the save card bottom sheet.
 */
@JNINamespace("autofill")
public class AutofillSaveCardBottomSheetBridge
        implements AutofillSaveCardBottomSheetCoordinator.NativeDelegate {
    private long mNativeAutofillSaveCardBottomSheetBridge;
    private final TabModel mTabModel;
    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final LayoutStateProvider mLayoutStateProvider;
    private AutofillSaveCardBottomSheetCoordinator mCoordinator;

    @CalledByNative
    @VisibleForTesting
    /*package*/ AutofillSaveCardBottomSheetBridge(
            long nativeAutofillSaveCardBottomSheetBridge, WindowAndroid window, TabModel tabModel) {
        mNativeAutofillSaveCardBottomSheetBridge = nativeAutofillSaveCardBottomSheetBridge;
        mTabModel = tabModel;
        mContext = window.getContext().get();
        mBottomSheetController = BottomSheetControllerProvider.from(window);
        mLayoutStateProvider = LayoutManagerProvider.from(window);
    }

    /**
     * Requests to show the bottom sheet.
     *
     * <p>The bottom sheet may not be shown in some cases. {@see
     * BottomSheetController#requestShowContent}
     *
     * @param uiInfo An object providing text and images to the bottom sheet view.
     * @param skipLoadingForFixFlow When true, loading is skipped due to the fix flow.
     */
    @CalledByNative
    public void requestShowContent(AutofillSaveCardUiInfo uiInfo, boolean skipLoadingForFixFlow) {
        if (mNativeAutofillSaveCardBottomSheetBridge == 0) return;
        mCoordinator =
                new AutofillSaveCardBottomSheetCoordinator(
                        mContext,
                        uiInfo,
                        skipLoadingForFixFlow,
                        mBottomSheetController,
                        mLayoutStateProvider,
                        mTabModel,
                        /* delegate= */ this);
        mCoordinator.requestShowContent();
    }

    /**
     * Requests to hide the bottom sheet if showing. The hide reason
     * BottomSheetController.StateChangeReason.INTERACTION_COMPLETE will be used.
     */
    @CalledByNative
    public void hide() {
        if (mNativeAutofillSaveCardBottomSheetBridge == 0) return;
        mCoordinator.hide(BottomSheetController.StateChangeReason.INTERACTION_COMPLETE);
    }

    /** Called when the bottom sheet has been shown. */
    @Override
    public void onUiShown() {
        if (mNativeAutofillSaveCardBottomSheetBridge == 0) return;
        AutofillSaveCardBottomSheetBridgeJni.get()
                .onUiShown(mNativeAutofillSaveCardBottomSheetBridge);
    }

    /** Called when the confirm button has been clicked. */
    @Override
    public void onUiAccepted() {
        if (mNativeAutofillSaveCardBottomSheetBridge == 0) return;
        AutofillSaveCardBottomSheetBridgeJni.get()
                .onUiAccepted(mNativeAutofillSaveCardBottomSheetBridge);
    }

    /** Called when the cancel button is pushed or bottom sheet dismissed (e.g. back press). */
    @Override
    public void onUiCanceled() {
        if (mNativeAutofillSaveCardBottomSheetBridge == 0) return;
        AutofillSaveCardBottomSheetBridgeJni.get()
                .onUiCanceled(mNativeAutofillSaveCardBottomSheetBridge);
    }

    /** Called when the the bottom sheet is hidden without interaction with the bottom sheet. */
    @Override
    public void onUiIgnored() {
        if (mNativeAutofillSaveCardBottomSheetBridge == 0) return;
        AutofillSaveCardBottomSheetBridgeJni.get()
                .onUiIgnored(mNativeAutofillSaveCardBottomSheetBridge);
    }

    @CalledByNative
    @VisibleForTesting
    /*package*/ void destroy() {
        mNativeAutofillSaveCardBottomSheetBridge = 0;
        if (mCoordinator == null) return;
        mCoordinator.hide(BottomSheetController.StateChangeReason.NONE);
        mCoordinator = null;
    }

    @NativeMethods
    public interface Natives {
        void onUiShown(long nativeAutofillSaveCardBottomSheetBridge);

        void onUiAccepted(long nativeAutofillSaveCardBottomSheetBridge);

        void onUiCanceled(long nativeAutofillSaveCardBottomSheetBridge);

        void onUiIgnored(long nativeAutofillSaveCardBottomSheetBridge);
    }
}
