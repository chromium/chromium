// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility_annotator;

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
 * Bridge class for AccessibilityAnnotatorBottomSheet.
 *
 * <p>Objects of this type are created and owned by the corresponding native implementation.
 */
@JNINamespace("accessibility_annotator")
@NullMarked
class AccessibilityAnnotatorBottomSheetBridge
        implements AccessibilityAnnotatorBottomSheetCoordinator.Delegate {
    private long mNativeBridge;
    private final AccessibilityAnnotatorBottomSheetCoordinator mCoordinator;

    @CalledByNative
    private static @Nullable AccessibilityAnnotatorBottomSheetBridge create(
            long nativeBridge, WindowAndroid windowAndroid) {
        BottomSheetController bottomSheetController =
                BottomSheetControllerProvider.from(windowAndroid);
        Context context = windowAndroid.getContext().get();

        if (context == null || bottomSheetController == null) {
            return null;
        }

        return new AccessibilityAnnotatorBottomSheetBridge(
                nativeBridge, context, bottomSheetController);
    }

    private AccessibilityAnnotatorBottomSheetBridge(
            long nativeBridge, Context context, BottomSheetController bottomSheetController) {
        mNativeBridge = nativeBridge;
        mCoordinator =
                new AccessibilityAnnotatorBottomSheetCoordinator(
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
    private boolean show() {
        return mCoordinator.requestShowContent();
    }

    @CalledByNative
    private void hide() {
        mCoordinator.hide(BottomSheetController.StateChangeReason.NONE);
    }

    @Override
    public void onInfoAcknowledged() {
        if (mNativeBridge != 0) {
            AccessibilityAnnotatorBottomSheetBridgeJni.get().onInfoAcknowledged(mNativeBridge);
        }
    }

    @Override
    public void onManageSettingsClicked() {
        if (mNativeBridge != 0) {
            AccessibilityAnnotatorBottomSheetBridgeJni.get().onManageSettingsClicked(mNativeBridge);
        }
    }

    @Override
    public void onLearnMoreClicked() {
        if (mNativeBridge != 0) {
            AccessibilityAnnotatorBottomSheetBridgeJni.get().onLearnMoreClicked(mNativeBridge);
        }
    }

    @Override
    public void onInfoDismissed() {
        if (mNativeBridge != 0) {
            AccessibilityAnnotatorBottomSheetBridgeJni.get().onInfoDismissed(mNativeBridge);
        }
    }

    @NativeMethods
    interface Natives {
        void onInfoAcknowledged(
                @JniType("accessibility_annotator::AccessibilityAnnotatorBottomSheetBridge*")
                        long nativeAccessibilityAnnotatorBottomSheetBridge);

        void onManageSettingsClicked(
                @JniType("accessibility_annotator::AccessibilityAnnotatorBottomSheetBridge*")
                        long nativeAccessibilityAnnotatorBottomSheetBridge);

        void onLearnMoreClicked(
                @JniType("accessibility_annotator::AccessibilityAnnotatorBottomSheetBridge*")
                        long nativeAccessibilityAnnotatorBottomSheetBridge);

        void onInfoDismissed(
                @JniType("accessibility_annotator::AccessibilityAnnotatorBottomSheetBridge*")
                        long nativeAccessibilityAnnotatorBottomSheetBridge);
    }
}
