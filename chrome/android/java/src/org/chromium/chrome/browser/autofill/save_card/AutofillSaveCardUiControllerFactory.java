// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.save_card;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;

@NullMarked
class AutofillSaveCardUiControllerFactory {
    private static class BottomSheetAdapter implements AutofillSaveCardUiController {
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

    /** Creates an AutofillSaveCardUiController which wraps the given BottomSheetController. */
    static AutofillSaveCardUiController createBottomSheetUiController(
            BottomSheetController controller) {
        return new BottomSheetAdapter(controller);
    }

    private static class AnchoredDialogAdapter implements AutofillSaveCardUiController {
        private final AnchoredDialogCoordinator mController;

        AnchoredDialogAdapter(AnchoredDialogCoordinator controller) {
            mController = controller;
        }

        @Override
        public boolean requestShowContent(BottomSheetContent content, boolean animate) {
            mController.requestShowContent(content);
            return true;
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

    /** Creates an AutofillSaveCardUiController which wraps the given AnchoredDialogCoordinator. */
    static AutofillSaveCardUiController createAnchoredDialogUiController(
            AnchoredDialogCoordinator controller) {
        return new AnchoredDialogAdapter(controller);
    }

    // Do not instantiate this class.
    private AutofillSaveCardUiControllerFactory() {}
}
