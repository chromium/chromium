// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.content.Context;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
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
public class AutofillSaveCardBottomSheetBridge {
    private long mNativeAutofillSaveCardBottomSheetBridge;
    private final TabModel mTabModel;
    private Context mContext;
    private BottomSheetController mBottomSheetController;
    private LayoutStateProvider mLayoutStateProvider;
    private CoordinatorFactory mCoordinatorFactory;
    @Nullable
    private AutofillSaveCardBottomSheetCoordinator mCoordinator;

    @CalledByNative
    @VisibleForTesting
    private AutofillSaveCardBottomSheetBridge(
            long nativeAutofillSaveCardBottomSheetBridge, WindowAndroid window, TabModel tabModel) {
        this(nativeAutofillSaveCardBottomSheetBridge, window, tabModel,
                AutofillSaveCardBottomSheetCoordinator::new);
    }

    @CalledByNative
    @VisibleForTesting
    /*package*/ AutofillSaveCardBottomSheetBridge(long nativeAutofillSaveCardBottomSheetBridge,
            WindowAndroid window, TabModel tabModel, CoordinatorFactory coordinatorFactory) {
        mContext = window.getContext().get();
        mNativeAutofillSaveCardBottomSheetBridge = nativeAutofillSaveCardBottomSheetBridge;
        mBottomSheetController = BottomSheetControllerProvider.from(window);
        mLayoutStateProvider = LayoutManagerProvider.from(window);
        mTabModel = tabModel;
        mCoordinatorFactory = coordinatorFactory;
    }

    @VisibleForTesting
    /*package*/ static interface CoordinatorFactory {
        AutofillSaveCardBottomSheetCoordinator create(Context context,
                BottomSheetController bottomSheetController,
                LayoutStateProvider layoutStateProvider, TabModel tabModel,
                AutofillSaveCardUiInfo uiInfo, AutofillSaveCardBottomSheetBridge bridge);
    }

    /**
     * Requests to show the bottom sheet.
     *
     * <p>The bottom sheet may not be shown in some cases. {@see
     * BottomSheetController#requestShowContent}
     *
     * @param uiInfo An object providing text and images to the bottom sheet view.
     */
    @CalledByNative
    public void requestShowContent(AutofillSaveCardUiInfo uiInfo) {
        if (mNativeAutofillSaveCardBottomSheetBridge != 0) {
            mCoordinator = mCoordinatorFactory.create(mContext, mBottomSheetController,
                    mLayoutStateProvider, mTabModel, uiInfo, this);
            mCoordinator.requestShowContent();
        }
    }

    @CalledByNative
    @VisibleForTesting
    /*package*/ void destroy() {
        if (mCoordinator != null) {
            mCoordinator.destroy();
            mCoordinator = null;
        }
        mNativeAutofillSaveCardBottomSheetBridge = 0;
    }

    /** Called when the bottom sheet has been shown. */
    public void onUiShown() {
        if (mNativeAutofillSaveCardBottomSheetBridge != 0) {
            AutofillSaveCardBottomSheetBridgeJni.get().onUiShown(
                    mNativeAutofillSaveCardBottomSheetBridge);
        }
    }

    /** Called when the confirm button as been clicked. */
    public void onUiAccepted() {
        if (mNativeAutofillSaveCardBottomSheetBridge != 0) {
            AutofillSaveCardBottomSheetBridgeJni.get().onUiAccepted(
                    mNativeAutofillSaveCardBottomSheetBridge);
        }
    }

    /** Called when the cancel button is pushed or bottom sheet dismissed (e.g. back press). */
    public void onUiCanceled() {
        if (mNativeAutofillSaveCardBottomSheetBridge != 0) {
            AutofillSaveCardBottomSheetBridgeJni.get().onUiCanceled(
                    mNativeAutofillSaveCardBottomSheetBridge);
        }
    }

    /** Called when the the bottom sheet is hidden without interaction with the bottom sheet. */
    public void onUiIgnored() {
        if (mNativeAutofillSaveCardBottomSheetBridge != 0) {
            AutofillSaveCardBottomSheetBridgeJni.get().onUiIgnored(
                    mNativeAutofillSaveCardBottomSheetBridge);
        }
    }

    @NativeMethods
    public interface Natives {
        void onUiShown(long nativeAutofillSaveCardBottomSheetBridge);

        void onUiAccepted(long nativeAutofillSaveCardBottomSheetBridge);

        void onUiCanceled(long nativeAutofillSaveCardBottomSheetBridge);

        void onUiIgnored(long nativeAutofillSaveCardBottomSheetBridge);
    }
}
