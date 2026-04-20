// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility_annotator.first_run;

import android.content.Context;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;

/**
 * Bridge class for AccessibilityAnnotatorFirstRunBottomSheet.
 *
 * <p>Objects of this type are created and owned by the corresponding native implementation.
 */
@JNINamespace("accessibility_annotator")
@NullMarked
class AccessibilityAnnotatorFirstRunBottomSheetBridge
        implements AccessibilityAnnotatorFirstRunBottomSheetCoordinator.Delegate {
    private long mNativeBridge;
    private final AccessibilityAnnotatorFirstRunBottomSheetCoordinator mCoordinator;

    @CalledByNative
    private static @Nullable AccessibilityAnnotatorFirstRunBottomSheetBridge create(
            long nativeBridge, WindowAndroid windowAndroid) {
        BottomSheetController bottomSheetController =
                BottomSheetControllerProvider.from(windowAndroid);
        Context context = windowAndroid.getContext().get();

        if (context == null || bottomSheetController == null) {
            return null;
        }

        return new AccessibilityAnnotatorFirstRunBottomSheetBridge(
                nativeBridge, context, bottomSheetController);
    }

    private AccessibilityAnnotatorFirstRunBottomSheetBridge(
            long nativeBridge, Context context, BottomSheetController bottomSheetController) {
        mNativeBridge = nativeBridge;
        mCoordinator =
                new AccessibilityAnnotatorFirstRunBottomSheetCoordinator(
                        context, bottomSheetController, this);
    }

    /**
     * The native counterpart calls this method prior to cleaning up its last reference to this
     * object so it can be correctly torn down.
     */
    @CalledByNative
    private void destroy() {
        mNativeBridge = 0;
    }

    @CalledByNative
    private boolean show(
            @JniType("std::string") String manageSettingsUrl,
            @JniType("std::string") String learnMoreUrl) {
        return mCoordinator.requestShowContent(manageSettingsUrl, learnMoreUrl);
    }

    @CalledByNative
    private void hide() {
        mCoordinator.hide(BottomSheetController.StateChangeReason.NONE);
    }

    @Override
    public void onInfoAcknowledged() {
        if (mNativeBridge != 0) {
            AccessibilityAnnotatorFirstRunBottomSheetBridgeJni.get()
                    .onInfoAcknowledged(mNativeBridge);
        }
    }

    @Override
    public void onManageSettingsClicked() {
        if (mNativeBridge != 0) {
            AccessibilityAnnotatorFirstRunBottomSheetBridgeJni.get()
                    .onManageSettingsClicked(mNativeBridge);
        }
    }

    @Override
    public void onLearnMoreClicked() {
        if (mNativeBridge != 0) {
            AccessibilityAnnotatorFirstRunBottomSheetBridgeJni.get()
                    .onLearnMoreClicked(mNativeBridge);
        }
    }

    @Override
    public void onInfoDismissed() {
        if (mNativeBridge != 0) {
            AccessibilityAnnotatorFirstRunBottomSheetBridgeJni.get().onInfoDismissed(mNativeBridge);
        }
    }

    @NativeMethods
    interface Natives {
        void onInfoAcknowledged(
                @JniType(
                                "accessibility_annotator::AccessibilityAnnotatorFirstRunBottomSheetBridge*")
                        long nativeAccessibilityAnnotatorFirstRunBottomSheetBridge);

        void onManageSettingsClicked(
                @JniType(
                                "accessibility_annotator::AccessibilityAnnotatorFirstRunBottomSheetBridge*")
                        long nativeAccessibilityAnnotatorFirstRunBottomSheetBridge);

        void onLearnMoreClicked(
                @JniType(
                                "accessibility_annotator::AccessibilityAnnotatorFirstRunBottomSheetBridge*")
                        long nativeAccessibilityAnnotatorFirstRunBottomSheetBridge);

        void onInfoDismissed(
                @JniType(
                                "accessibility_annotator::AccessibilityAnnotatorFirstRunBottomSheetBridge*")
                        long nativeAccessibilityAnnotatorFirstRunBottomSheetBridge);
    }
}
