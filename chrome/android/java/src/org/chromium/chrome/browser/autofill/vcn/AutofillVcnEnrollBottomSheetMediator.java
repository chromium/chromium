// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.vcn;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;

/** The mediator controller for the virtual card number (VCN) enrollment bottom sheet. */
/*package*/ class AutofillVcnEnrollBottomSheetMediator {
    private final AutofillVcnEnrollBottomSheetContent mContent;
    private final AutofillVcnEnrollBottomSheetLifecycle mLifecycle;
    private BottomSheetController mBottomSheetController;

    /**
     * Constructs the mediator controller for the virtual card enrollment bottom sheet.
     *
     * @param content The bottom sheet content.
     * @param lifecycle A custom lifecycle that ignores page navigations.
     */
    AutofillVcnEnrollBottomSheetMediator(AutofillVcnEnrollBottomSheetContent content,
            AutofillVcnEnrollBottomSheetLifecycle lifecycle) {
        mContent = content;
        mLifecycle = lifecycle;
    }

    /**
     * Requests to show the bottom sheet.
     *
     * @param window The window where the bottom sheet should be shown.
     *
     * @return True if shown.
     */
    boolean requestShowContent(WindowAndroid window) {
        if (!mLifecycle.canBegin()) return false;

        mBottomSheetController = BottomSheetControllerProvider.from(window);
        if (mBottomSheetController == null) return false;

        boolean didShow = mBottomSheetController.requestShowContent(mContent, /*animate=*/true);

        if (didShow) mLifecycle.begin(/*onEndOfLifecycle=*/this::hide);

        return didShow;
    }

    /** Hides the bottom sheet, if present. */
    void hide() {
        if (mLifecycle.hasBegun()) mLifecycle.end();

        if (mBottomSheetController == null) return;
        mBottomSheetController.hideContent(mContent, /*animate=*/true,
                BottomSheetController.StateChangeReason.INTERACTION_COMPLETE);
    }
}
