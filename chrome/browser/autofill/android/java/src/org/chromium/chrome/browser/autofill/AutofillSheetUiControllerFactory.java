// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.autofill.anchored_dialog.AnchoredDialogCoordinator;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.ui.base.DeviceFormFactor;

@NullMarked
public class AutofillSheetUiControllerFactory {
    private static class BottomSheetAdapter implements AutofillSheetUiController {
        private final BottomSheetController mController;

        BottomSheetAdapter(BottomSheetController controller) {
            mController = controller;
        }

        @Override
        public boolean requestShowContent(BottomSheetContent content, boolean animate) {
            return mController.requestShowContent(content, animate);
        }

        @Override
        public void hideContent(
                @Nullable BottomSheetContent content,
                boolean animate,
                @BottomSheetController.StateChangeReason int hideReason) {
            mController.hideContent(content, animate, hideReason);
        }

        @Override
        public void addObserver(BottomSheetObserver observer) {
            mController.addObserver(observer);
        }

        @Override
        public void removeObserver(BottomSheetObserver observer) {
            mController.removeObserver(observer);
        }
    }

    /** Creates an AutofillSheetUiController which wraps the given BottomSheetController. */
    public static AutofillSheetUiController createBottomSheetUiController(
            BottomSheetController controller) {
        return new BottomSheetAdapter(controller);
    }

    private static class AnchoredDialogAdapter implements AutofillSheetUiController {
        private final AnchoredDialogCoordinator mController;

        AnchoredDialogAdapter(AnchoredDialogCoordinator controller) {
            mController = controller;
        }

        @Override
        public boolean requestShowContent(BottomSheetContent content, boolean animate) {
            return mController.requestShowContent(content);
        }

        @Override
        public void hideContent(
                @Nullable BottomSheetContent content,
                boolean animate,
                @StateChangeReason int hideReason) {
            mController.hideContent(content, hideReason);
        }

        @Override
        public void addObserver(BottomSheetObserver observer) {
            mController.addObserver(observer);
        }

        @Override
        public void removeObserver(BottomSheetObserver observer) {
            mController.removeObserver(observer);
        }
    }

    /** Creates an AutofillSheetUiController which wraps the given AnchoredDialogCoordinator. */
    public static AutofillSheetUiController createAnchoredDialogUiController(
            AnchoredDialogCoordinator controller) {
        return new AnchoredDialogAdapter(controller);
    }

    public static boolean shouldUseNonBlockingDialog(Context context) {
        return DeviceFormFactor.isNonMultiDisplayContextOnTablet(context)
                && ChromeFeatureList.isEnabled(
                        ChromeFeatureList.ANDROID_SAVE_CARD_NON_BLOCKING_DIALOG);
    }

    public static AutofillSheetUiController createUiController(
            Context context,
            BottomSheetController bottomSheetController,
            AnchoredDialogCoordinator anchoredDialogController) {
        return shouldUseNonBlockingDialog(context)
                ? createAnchoredDialogUiController(anchoredDialogController)
                : createBottomSheetUiController(bottomSheetController);
    }

    // Do not instantiate this class.
    private AutofillSheetUiControllerFactory() {}
}
