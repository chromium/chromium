// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.vcn;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

/** The mediator controller for the virtual card number (VCN) enrollment bottom sheet. */
/*package*/ class AutofillVcnEnrollBottomSheetMediator {
    private final AutofillVcnEnrollBottomSheetContent mContent;
    private final AutofillVcnEnrollBottomSheetLifecycle mLifecycle;
    private BottomSheetController mBottomSheetController;
    private final PropertyModel mModel;

    /**
     * Constructs the mediator controller for the virtual card enrollment bottom sheet.
     *
     * @param content The bottom sheet content.
     * @param lifecycle A custom lifecycle that ignores page navigations.
     */
    AutofillVcnEnrollBottomSheetMediator(
            AutofillVcnEnrollBottomSheetContent content,
            AutofillVcnEnrollBottomSheetLifecycle lifecycle,
            PropertyModel model) {
        mContent = content;
        mLifecycle = lifecycle;
        mModel = model;
    }

    /**
     * Requests to show the bottom sheet.
     *
     * @param window The window where the bottom sheet should be shown.
     * @return True if shown.
     */
    boolean requestShowContent(WindowAndroid window) {
        if (!mLifecycle.canBegin()) return false;

        mBottomSheetController = BottomSheetControllerProvider.from(window);
        if (mBottomSheetController == null) return false;

        boolean didShow = mBottomSheetController.requestShowContent(mContent, /* animate= */ true);

        if (didShow) mLifecycle.begin(/* onEndOfLifecycle= */ this::hide);

        return didShow;
    }

    /** Callback for when the user hits the [accept] button. */
    void onAccept() {
        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.AUTOFILL_ENABLE_VCN_ENROLL_LOADING_AND_CONFIRMATION)) {
            mModel.set(AutofillVcnEnrollBottomSheetProperties.SHOW_LOADING_STATE, true);
        } else {
            hide();
        }
    }

    /** Callback for when the user hits the [cancel] button. */
    void onCancel() {
        hide();
    }

    /** Hides the bottom sheet, if present. */
    void hide() {
        if (mLifecycle.hasBegun()) mLifecycle.end();

        if (mBottomSheetController == null) return;
        mBottomSheetController.hideContent(
                mContent,
                /* animate= */ true,
                BottomSheetController.StateChangeReason.INTERACTION_COMPLETE);
    }
}
