// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.vcn;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

/** The mediator controller for the virtual card number (VCN) enrollment bottom sheet. */
/*package*/ class AutofillVcnEnrollBottomSheetMediator {
    @VisibleForTesting
    static final String LOADING_SHOWN_HISTOGRAM = "Autofill.VirtualCardEnrollBubble.LoadingShown";

    @VisibleForTesting
    static final String LOADING_RESULT_HISTOGRAM = "Autofill.VirtualCardEnrollBubble.LoadingResult";

    private final AutofillVcnEnrollBottomSheetContent mContent;
    private final AutofillVcnEnrollBottomSheetLifecycle mLifecycle;
    private BottomSheetController mBottomSheetController;
    private final PropertyModel mModel;
    private @VirtualCardEnrollmentBubbleResult int mLoadingResult;

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    // Needs to stay in sync with AutofillVirtualCardEnrollBubbleResult in enums.xml.
    @IntDef({
        VirtualCardEnrollmentBubbleResult.UNKNOWN,
        VirtualCardEnrollmentBubbleResult.ACCEPTED,
        VirtualCardEnrollmentBubbleResult.CLOSED,
        VirtualCardEnrollmentBubbleResult.NOT_INTERACTED,
        VirtualCardEnrollmentBubbleResult.LOST_FOCUS,
        VirtualCardEnrollmentBubbleResult.CANCELLED,
        VirtualCardEnrollmentBubbleResult.COUNT
    })
    @VisibleForTesting
    @interface VirtualCardEnrollmentBubbleResult {
        int UNKNOWN = 0;
        int ACCEPTED = 1;
        int CLOSED = 2;
        int NOT_INTERACTED = 3;
        int LOST_FOCUS = 4;
        int CANCELLED = 5;
        int COUNT = 6;
    }

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
            mLoadingResult = VirtualCardEnrollmentBubbleResult.ACCEPTED;
            RecordHistogram.recordBooleanHistogram(LOADING_SHOWN_HISTOGRAM, true);
        } else {
            hide();
        }
    }

    /** Callback for when the user hits the [cancel] button. */
    void onCancel() {
        if (mModel.get(AutofillVcnEnrollBottomSheetProperties.SHOW_LOADING_STATE)) {
            mLoadingResult = VirtualCardEnrollmentBubbleResult.CLOSED;
        }
        hide();
    }

    /** Hides the bottom sheet, if present. */
    void hide() {
        if (mLifecycle.hasBegun()) mLifecycle.end();
        if (mBottomSheetController != null) {
            mBottomSheetController.hideContent(
                    mContent,
                    /* animate= */ true,
                    BottomSheetController.StateChangeReason.INTERACTION_COMPLETE);
        }
        if (mModel.get(AutofillVcnEnrollBottomSheetProperties.SHOW_LOADING_STATE)) {
            // Reset loading state to false to prevent a race condition from recording the metric
            // twice.
            mModel.set(AutofillVcnEnrollBottomSheetProperties.SHOW_LOADING_STATE, false);
            RecordHistogram.recordEnumeratedHistogram(
                    LOADING_RESULT_HISTOGRAM,
                    mLoadingResult,
                    VirtualCardEnrollmentBubbleResult.COUNT);
        }
    }
}
