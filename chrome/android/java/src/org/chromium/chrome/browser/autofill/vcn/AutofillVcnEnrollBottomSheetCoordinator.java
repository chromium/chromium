// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.vcn;

import android.content.Context;

import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator controller for the virtual card enrollment bottom sheet. */
/*package*/ class AutofillVcnEnrollBottomSheetCoordinator {
    private final AutofillVcnEnrollBottomSheetMediator mMediator;

    /**
     * Constructs a coordinator controller for the virtual card enrollment bottom sheet.
     *
     * @param context The activity context.
     * @param messageText The prompt message for the bottom sheet, e.g., "Make it more secure with a
     *                    virtual card next time?"
     * @param acceptButtonLabel The label for the button that enrolls a virtual card.
     * @param cancelButtonLabel The label for the button that cancels enrollment.
     * @param onAccept The callback to invoke when the user accepts the enrollment prompt.
     * @param onCancel The callback to invoke when the user cancels the enrollment prompt.
     * @param onDismiss The callback to invoke when the user dismisses the bottom sheet.
     */
    /*package*/ AutofillVcnEnrollBottomSheetCoordinator(Context context, String messageText,
            String acceptButtonLabel, String cancelButtonLabel, Runnable onAccept,
            Runnable onCancel, Runnable onDismiss) {
        PropertyModel model =
                new PropertyModel.Builder(AutofillVcnEnrollBottomSheetProperties.ALL_KEYS)
                        .with(AutofillVcnEnrollBottomSheetProperties.MESSAGE_TEXT, messageText)
                        .with(AutofillVcnEnrollBottomSheetProperties.ACCEPT_BUTTON_LABEL,
                                acceptButtonLabel)
                        .with(AutofillVcnEnrollBottomSheetProperties.CANCEL_BUTTON_LABEL,
                                cancelButtonLabel)
                        .build();

        AutofillVcnEnrollBottomSheetView view = new AutofillVcnEnrollBottomSheetView(context);

        PropertyModelChangeProcessor.create(
                model, view, AutofillVcnEnrollBottomSheetViewBinder::bind);

        mMediator = new AutofillVcnEnrollBottomSheetMediator(view.mContentView, view.mAcceptButton,
                view.mCancelButton, onAccept, onCancel, onDismiss);
    }

    /**
     * Requests to show the bottom sheet.
     *
     * @param window The window where the bottom sheet should be shown.
     *
     * @return True if shown.
     */
    /*package*/ boolean requestShowContent(WindowAndroid window) {
        return mMediator.requestShowContent(window);
    }

    /** Hides the virtual card enrollment bottom sheet, if present. */
    /*package*/ void hide() {
        mMediator.hide();
    }
}
