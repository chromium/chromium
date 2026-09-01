// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.autofill;

import android.content.Context;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.touch_to_fill.common.BottomSheetFocusHelper;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.content_public.browser.ImeAdapter;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;

/** JNI wrapper for C++ TouchToFillAutofillViewImpl. Delegates calls from native to Java. */
@JNINamespace("autofill")
@NullMarked
class TouchToFillAutofillViewBridge implements TouchToFillAutofillComponent.Delegate {
    private final TouchToFillAutofillComponent mComponent;
    private long mNativeViewImpl;
    private final WindowAndroid mWindowAndroid;
    private final BottomSheetController mBottomSheetController;
    private final WebContents mWebContents;
    private boolean mIsObserverRegistered;
    private final BottomSheetObserver mBottomSheetObserver =
            new BottomSheetObserver() {
                @Override
                public void onSheetClosed(@StateChangeReason int reason) {
                    if (reason != StateChangeReason.OMNIBOX_FOCUS
                            && reason != StateChangeReason.NAVIGATION) {
                        restoreKeyboardAndFocus();
                    }
                    if (mNativeViewImpl != 0) {
                        TouchToFillAutofillViewBridgeJni.get().onDismissed(mNativeViewImpl);
                    }
                    mBottomSheetController.removeObserver(this);
                    mIsObserverRegistered = false;
                }
            };

    @VisibleForTesting
    public TouchToFillAutofillViewBridge(
            long nativeViewImpl,
            Context context,
            BottomSheetController bottomSheetController,
            WindowAndroid windowAndroid,
            WebContents webContents) {
        mNativeViewImpl = nativeViewImpl;
        mWindowAndroid = windowAndroid;
        mBottomSheetController = bottomSheetController;
        mWebContents = webContents;
        mComponent =
                new TouchToFillAutofillCoordinator(
                        context,
                        bottomSheetController,
                        this,
                        new BottomSheetFocusHelper(bottomSheetController, windowAndroid));
    }

    @CalledByNative
    private static @Nullable TouchToFillAutofillViewBridge create(
            long nativeViewImpl, @Nullable WindowAndroid windowAndroid, WebContents webContents) {
        if (windowAndroid == null) return null;
        Context context = windowAndroid.getContext().get();
        if (context == null) return null;
        BottomSheetController bottomSheetController =
                BottomSheetControllerProvider.from(windowAndroid);
        if (bottomSheetController == null) return null;
        return new TouchToFillAutofillViewBridge(
                nativeViewImpl, context, bottomSheetController, windowAndroid, webContents);
    }

    @CalledByNative
    void show() {
        if (!mIsObserverRegistered) {
            mBottomSheetController.addObserver(mBottomSheetObserver);
            mIsObserverRegistered = true;
        }
        mComponent.show();
    }

    @CalledByNative
    private void hide() {
        mComponent.hide();
    }

    @Override
    public void onNoticeAcknowledged() {
        if (mNativeViewImpl != 0) {
            TouchToFillAutofillViewBridgeJni.get().onNoticeAcknowledged(mNativeViewImpl);
        }
    }

    @Override
    public void onSettingsLinkClicked() {
        if (mNativeViewImpl != 0) {
            TouchToFillAutofillViewBridgeJni.get().onSettingsLinkClicked(mNativeViewImpl);
        }
    }

    @Override
    public void onDismissed() {}

    /**
     * Restores system keyboard focus to the input container and notifies the native view delegate
     * that the bottom sheet was dismissed. Invoked asynchronously to prevent the rising keyboard
     * from overlapping with the sheet slide-down animation windows.
     */
    private void restoreKeyboardAndFocus() {
        if (mNativeViewImpl == 0) return;
        if (mWebContents.isDestroyed()) return;

        ImeAdapter imeAdapter = ImeAdapter.fromWebContents(mWebContents);
        if (imeAdapter != null) {
            imeAdapter.setKeyboardSuppressed(false);
        }

        ViewAndroidDelegate viewDelegate = mWebContents.getViewAndroidDelegate();
        if (viewDelegate == null) return;

        View containerView = viewDelegate.getContainerView();
        if (containerView == null) return;

        if (!containerView.isFocused()) {
            containerView.requestFocus();
        }
        mWindowAndroid.getKeyboardDelegate().showKeyboard(containerView);
    }

    @CalledByNative
    void destroy() {
        if (mNativeViewImpl == 0) {
            return;
        }
        mNativeViewImpl = 0;
        if (mIsObserverRegistered) {
            mBottomSheetController.removeObserver(mBottomSheetObserver);
            mIsObserverRegistered = false;
        }
        mComponent.destroy();
    }

    @NativeMethods
    interface Natives {
        void onNoticeAcknowledged(long nativeTouchToFillAutofillViewImpl);

        void onSettingsLinkClicked(long nativeTouchToFillAutofillViewImpl);

        void onDismissed(long nativeTouchToFillAutofillViewImpl);
    }
}
