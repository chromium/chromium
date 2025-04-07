// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.no_passkeys;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

import java.lang.ref.WeakReference;

/** Mediator class for the no passkeys bottom sheet. */
@NullMarked
class NoPasskeysBottomSheetMediator implements NoPasskeysBottomSheetContent.Delegate {
    private final WeakReference<BottomSheetController> mBottomSheetController;

    private @Nullable BottomSheetContent mBottomSheetContent;
    private NoPasskeysBottomSheetCoordinator.@Nullable NativeDelegate mNativeDelegate;

    /**
     * Creates the mediator.
     *
     * @param bottomSheetController The controller to use for showing or hiding the content.
     * @param nativeDelegate A NoPasskeysBottomSheetBridge to interact with the native side.
     */
    NoPasskeysBottomSheetMediator(
            WeakReference<BottomSheetController> bottomSheetController,
            NoPasskeysBottomSheetCoordinator.NativeDelegate nativeDelegate) {
        mBottomSheetController = bottomSheetController;
        mNativeDelegate = nativeDelegate;
    }

    /**
     * Requests to show the bottom sheet content. If the environment prevents showing, the method
     * will return early (e.g. because the finishing activity cleans up UI components).
     *
     * @param bottomSheetContent The {@link NoPasskeysBottomSheetContent} to be shown.
     * @return True iff the bottomsheet is shown by the controller
     */
    boolean show(@Nullable BottomSheetContent bottomSheetContent) {
        assert bottomSheetContent != null;
        BottomSheetController bottomSheetController = mBottomSheetController.get();
        if (bottomSheetController == null) {
            return false;
        }
        mBottomSheetContent = bottomSheetContent;
        return bottomSheetController.requestShowContent(mBottomSheetContent, true);
    }

    /**
     * Hide the bottom sheet (if showing) and ensures the dismiss callback is
     * called.
     */
    void destroy() {
        dismiss();
        mBottomSheetContent = null;
    }

    // NoPasskeysBottomSheetContent.Delegate implementation follows:
    @Override
    public void onClickOk() {
        dismiss();
    }

    @Override
    public void onClickUseAnotherDevice() {
        if (mNativeDelegate == null) {
            return;
        }
        mNativeDelegate.onClickUseAnotherDevice();
        dismiss();
    }

    @Override
    public void onDestroy() {
        destroy();
    }

    private void dismiss() {
        if (mNativeDelegate == null) {
            return; // Dismissal already happened.
        }
        mNativeDelegate.onDismissed();
        mNativeDelegate = null; // Don't call the callback repeatedly!
        BottomSheetController bottomSheetController = mBottomSheetController.get();
        assumeNonNull(bottomSheetController);
        assert mBottomSheetContent != null;
        bottomSheetController.hideContent(mBottomSheetContent, true);
    }
}
