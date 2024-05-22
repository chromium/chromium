// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.iban;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.ui.base.WindowAndroid;

/** JNI wrapper to trigger Android bottom sheet prompting the user to save their IBAN locally. */
@JNINamespace("autofill")
public class AutofillSaveIbanBottomSheetBridge {
    private long mNativeAutofillSaveIbanBottomSheetBridge;
    private CoordinatorFactory mCoordinatorFactory;
    private WindowAndroid mWindow;
    private TabModel mTabModel;
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
    private AutofillSaveIbanBottomSheetBridge(
            long nativeAutofillSaveIbanBottomSheetBridge, WindowAndroid window, TabModel tabModel) {
        this(nativeAutofillSaveIbanBottomSheetBridge, AutofillSaveIbanBottomSheetCoordinator::new);
        mWindow = window;
        mTabModel = tabModel;
    }

    @VisibleForTesting
    /*package*/ AutofillSaveIbanBottomSheetBridge(
            long nativeAutofillSaveIbanBottomSheetBridge, CoordinatorFactory coordinatorFactory) {
        mNativeAutofillSaveIbanBottomSheetBridge = nativeAutofillSaveIbanBottomSheetBridge;
        mCoordinatorFactory = coordinatorFactory;
    }

    @VisibleForTesting
    /*package*/ static interface CoordinatorFactory {
        AutofillSaveIbanBottomSheetCoordinator create(AutofillSaveIbanBottomSheetBridge bridge);
    }

    /**
     * Requests to show the bottom sheet. Called via JNI from C++.
     *
     * @param ibanLabel String value of the IBAN being saved, i.e. CH56 0483 5012 3456 7800 9.
     */
    @CalledByNative
    public void requestShowContent(@JniType("std::u16string_view") String ibanLabel) {
        if (mNativeAutofillSaveIbanBottomSheetBridge != 0) {
            mCoordinator = mCoordinatorFactory.create(this);
            mCoordinator.requestShowContent(ibanLabel);
        }
    }

    @CalledByNative
    @VisibleForTesting
    /*package*/ void destroy() {
        if (mCoordinator != null) {
            mCoordinator.destroy();
            mCoordinator = null;
        }
        mNativeAutofillSaveIbanBottomSheetBridge = 0;
    }

    /**
     * Called when the save button has been clicked.
     *
     * @param userProvidedNickname The nickname provided by the user when the "Save" button is
     *     clicked.
     */
    public void onUiAccepted(String userProvidedNickname) {
        if (mNativeAutofillSaveIbanBottomSheetBridge != 0) {
            AutofillSaveIbanBottomSheetBridgeJni.get()
                    .onUiAccepted(mNativeAutofillSaveIbanBottomSheetBridge, userProvidedNickname);
        }
    }

    /** Called when the cancel button is clicked or bottom sheet dismissed (e.g. back press). */
    public void onUiCanceled() {
        if (mNativeAutofillSaveIbanBottomSheetBridge != 0) {
            AutofillSaveIbanBottomSheetBridgeJni.get()
                    .onUiCanceled(mNativeAutofillSaveIbanBottomSheetBridge);
        }
    }

    /** Called when the the bottom sheet is hidden without interaction with the bottom sheet. */
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
