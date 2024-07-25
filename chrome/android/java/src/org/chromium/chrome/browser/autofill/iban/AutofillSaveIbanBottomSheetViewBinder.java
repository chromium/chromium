// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.iban;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * This class is responsible for binding and translating the IBAN model defined in
 * AutofillSaveIbanBottomSheetProperties to an AutofillSaveIbanBottomSheetView View.
 */
/*package*/ class AutofillSaveIbanBottomSheetViewBinder {
    static void bind(
            PropertyModel model, AutofillSaveIbanBottomSheetView view, PropertyKey propertyKey) {
        if (AutofillSaveIbanBottomSheetProperties.TITLE == propertyKey) {
            view.mTitle.setText(model.get(AutofillSaveIbanBottomSheetProperties.TITLE));
        } else if (AutofillSaveIbanBottomSheetProperties.IBAN_LABEL == propertyKey) {
            view.mIbanLabel.setText(model.get(AutofillSaveIbanBottomSheetProperties.IBAN_LABEL));
        } else if (AutofillSaveIbanBottomSheetProperties.ACCEPT_BUTTON_LABEL == propertyKey) {
            view.mAcceptButton.setText(
                    model.get(AutofillSaveIbanBottomSheetProperties.ACCEPT_BUTTON_LABEL));
        } else if (AutofillSaveIbanBottomSheetProperties.CANCEL_BUTTON_LABEL == propertyKey) {
            view.mCancelButton.setText(
                    model.get(AutofillSaveIbanBottomSheetProperties.CANCEL_BUTTON_LABEL));
        } else if (AutofillSaveIbanBottomSheetProperties.ON_ACCEPT_BUTTON_CLICK_ACTION
                == propertyKey) {
            view.mAcceptButton.setOnClickListener(
                    model.get(AutofillSaveIbanBottomSheetProperties.ON_ACCEPT_BUTTON_CLICK_ACTION));
        } else if (AutofillSaveIbanBottomSheetProperties.ON_CANCEL_BUTTON_CLICK_ACTION
                == propertyKey) {
            view.mCancelButton.setOnClickListener(
                    model.get(AutofillSaveIbanBottomSheetProperties.ON_CANCEL_BUTTON_CLICK_ACTION));
        }
    }
}
