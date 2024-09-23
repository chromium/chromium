// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.iban;

import android.content.Context;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.layouts.LayoutManagerProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.autofill.payments.AutofillSaveIbanUiInfo;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;

/** JNI wrapper to trigger Android bottom sheet prompting the user to save their IBAN locally. */
@JNINamespace("autofill")
public class AutofillSaveIbanBottomSheetBridge
        implements AutofillSaveIbanBottomSheetCoordinator.NativeDelegate {
    private long mNativeAutofillSaveIbanBottomSheetBridge;
    private final BottomSheetController mBottomSheetController;
    private final Context mContext;
    private final LayoutStateProvider mLayoutStateProvider;
    private final TabModel mTabModel;
    @Nullable private AutofillSaveIbanBottomSheetCoordinator mCoordinator;

    /**
     * Creates the bridge.
     *
     * @param nativeAutofillSaveIbanBottomSheetBridge The bridge to trigger UI flow events
     *     (OnUiCanceled, OnUiAccepted, etc.).
     * @param window The window where the bottom sheet should be shown.
     * @param tabModel The TabModel used to detect when the bottom sheet needs to be hidden after a
     *     tab change.
     */
    @CalledByNative
    @VisibleForTesting
    /*package*/ AutofillSaveIbanBottomSheetBridge(
            long nativeAutofillSaveIbanBottomSheetBridge, WindowAndroid window, TabModel tabModel) {
        mNativeAutofillSaveIbanBottomSheetBridge = nativeAutofillSaveIbanBottomSheetBridge;
        mBottomSheetController = BottomSheetControllerProvider.from(window);
        mContext = window.getContext().get();
        mLayoutStateProvider = LayoutManagerProvider.from(window);
        mTabModel = tabModel;
    }

    /**
     * Requests to show the bottom sheet. Called via JNI from C++.
     *
     * @param uiInfo An object providing UI resources to the bottom sheet view.
     */
    @CalledByNative
    public void requestShowContent(AutofillSaveIbanUiInfo uiInfo) {
        if (mNativeAutofillSaveIbanBottomSheetBridge == 0) return;
        mCoordinator =
                new AutofillSaveIbanBottomSheetCoordinator(
                        this,
                        uiInfo,
                        mContext,
                        mBottomSheetController,
                        mLayoutStateProvider,
                        mTabModel);
        mCoordinator.requestShowContent();
    }

    @CalledByNative
    @VisibleForTesting
    /*package*/ void destroy() {
        mNativeAutofillSaveIbanBottomSheetBridge = 0;
        if (mCoordinator == null) return;
        mCoordinator.destroy();
        mCoordinator = null;
    }

    /**
     * Called when the save button has been clicked.
     *
     * @param userProvidedNickname The nickname provided by the user when the "Save" button is
     *     clicked.
     */
    @Override
    public void onUiAccepted(String userProvidedNickname) {
        if (mNativeAutofillSaveIbanBottomSheetBridge != 0) {
            AutofillSaveIbanBottomSheetBridgeJni.get()
                    .onUiAccepted(mNativeAutofillSaveIbanBottomSheetBridge, userProvidedNickname);
        }
    }

    /** Called when the cancel button is clicked or bottom sheet dismissed (e.g. back press). */
    @Override
    public void onUiCanceled() {
        if (mNativeAutofillSaveIbanBottomSheetBridge != 0) {
            AutofillSaveIbanBottomSheetBridgeJni.get()
                    .onUiCanceled(mNativeAutofillSaveIbanBottomSheetBridge);
        }
    }

    /** Called when the the bottom sheet is hidden without interaction with the bottom sheet. */
    @Override
    public void onUiIgnored() {
        if (mNativeAutofillSaveIbanBottomSheetBridge != 0) {
            AutofillSaveIbanBottomSheetBridgeJni.get()
                    .onUiIgnored(mNativeAutofillSaveIbanBottomSheetBridge);
        }
    }

    @NativeMethods
    public interface Natives {
        void onUiAccepted(
                long nativeAutofillSaveIbanBottomSheetBridge,
                @JniType("std::u16string") String userProvidedNickname);

        void onUiCanceled(long nativeAutofillSaveIbanBottomSheetBridge);

        void onUiIgnored(long nativeAutofillSaveIbanBottomSheetBridge);
    }
}
