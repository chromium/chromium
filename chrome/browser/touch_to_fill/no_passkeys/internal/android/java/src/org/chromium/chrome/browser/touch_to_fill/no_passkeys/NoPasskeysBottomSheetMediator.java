// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.no_passkeys;

import androidx.annotation.Nullable;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

import java.lang.ref.WeakReference;

/**
 * Mediator class for the no passkeys bottom sheet.
 */
class NoPasskeysBottomSheetMediator implements NoPasskeysBottomSheetContent.Delegate {
    private final WeakReference<BottomSheetController> mBottomSheetController;

    private @Nullable BottomSheetContent mBottomSheetContent;
    private @Nullable Runnable mOnDismissed;

    /**
     * Creates the mediator.
     *
     * @param bottomSheetController The controller to use for showing or hiding the
     *                              content.
     * @param onDismissed           A runnable to clean up when the bottom sheet is
     *                              finally dismissed.
     */
    NoPasskeysBottomSheetMediator(
            WeakReference<BottomSheetController> bottomSheetController, Runnable onDismissed) {
        mBottomSheetController = bottomSheetController;
        mOnDismissed = onDismissed;
    }

    /**
     * Requests to show the bottom sheet content. If the environment prevents
     * showing, the method
     * will return early (e.g. because the finishing activity cleans up UI
     * components.
     *
     * @param bottomSheetContent The {@link NoPasskeysBottomSheetContent} to be
     *                           shown.
     * @return True iff the bottomsheet is shown by the controller
     */
    boolean show(@Nullable BottomSheetContent bottomSheetContent) {
        assert bottomSheetContent != null;
        if (mBottomSheetController.get() == null) {
            return false;
        }
        mBottomSheetContent = bottomSheetContent;
        return mBottomSheetController.get().requestShowContent(mBottomSheetContent, true);
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
        // TODO(crbug/1481495): Implement flow initiation — native or in java.
        dismiss();
    }

    @Override
    public void onDestroy() {
        destroy();
    }

    private void dismiss() {
        if (mOnDismissed == null) {
            return; // Dismissal already happened.
        }
        mBottomSheetController.get().hideContent(mBottomSheetContent, true);
        mOnDismissed.run();
        mOnDismissed = null; // Don't call the callback repeatedly!
    }
}
