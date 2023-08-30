// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.vcn;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** The view-binder of the autofill virtual card enrollment bottom sheet UI. */
/*package*/ abstract class AutofillVcnEnrollBottomSheetViewBinder {
    /**
     * Updates the view based on changes in the model.
     *
     * @param model The updated model to read.
     * @param view The view to update.
     * @param propertyKey The property of the model that has changed.
     */
    /*package*/ static void bind(
            PropertyModel model, AutofillVcnEnrollBottomSheetView view, PropertyKey propertyKey) {
        if (AutofillVcnEnrollBottomSheetProperties.MESSAGE_TEXT == propertyKey) {
            view.mDialogTitle.setText(
                    model.get(AutofillVcnEnrollBottomSheetProperties.MESSAGE_TEXT));

        } else if (AutofillVcnEnrollBottomSheetProperties.ACCEPT_BUTTON_LABEL == propertyKey) {
            view.mAcceptButton.setText(
                    model.get(AutofillVcnEnrollBottomSheetProperties.ACCEPT_BUTTON_LABEL));

        } else if (AutofillVcnEnrollBottomSheetProperties.CANCEL_BUTTON_LABEL == propertyKey) {
            view.mCancelButton.setText(
                    model.get(AutofillVcnEnrollBottomSheetProperties.CANCEL_BUTTON_LABEL));
        }
    }
}
