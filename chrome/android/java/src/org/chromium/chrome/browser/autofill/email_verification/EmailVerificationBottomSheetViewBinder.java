// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.email_verification;

import static org.chromium.chrome.browser.autofill.email_verification.EmailVerificationBottomSheetProperties.CANCEL_BUTTON_LABEL;
import static org.chromium.chrome.browser.autofill.email_verification.EmailVerificationBottomSheetProperties.CONFIRM_BUTTON_LABEL;
import static org.chromium.chrome.browser.autofill.email_verification.EmailVerificationBottomSheetProperties.DESCRIPTION;
import static org.chromium.chrome.browser.autofill.email_verification.EmailVerificationBottomSheetProperties.DRAG_HANDLE_VISIBLE;
import static org.chromium.chrome.browser.autofill.email_verification.EmailVerificationBottomSheetProperties.ON_CANCEL_CLICKED;
import static org.chromium.chrome.browser.autofill.email_verification.EmailVerificationBottomSheetProperties.ON_CONFIRM_CLICKED;
import static org.chromium.chrome.browser.autofill.email_verification.EmailVerificationBottomSheetProperties.TITLE;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** View binder for the email verification bottom sheet. */
@NullMarked
/*package*/ class EmailVerificationBottomSheetViewBinder {
    static void bind(
            PropertyModel model, EmailVerificationBottomSheetView view, PropertyKey propertyKey) {
        if (propertyKey == TITLE) {
            view.mTitle.setText(model.get(TITLE));
        } else if (propertyKey == DESCRIPTION) {
            view.mDescription.setText(model.get(DESCRIPTION));
        } else if (propertyKey == CONFIRM_BUTTON_LABEL) {
            view.mConfirmButton.setText(model.get(CONFIRM_BUTTON_LABEL));
        } else if (propertyKey == CANCEL_BUTTON_LABEL) {
            view.mCancelButton.setText(model.get(CANCEL_BUTTON_LABEL));
        } else if (propertyKey == DRAG_HANDLE_VISIBLE) {
            view.mDragHandler.setVisibility(
                    model.get(DRAG_HANDLE_VISIBLE) ? View.VISIBLE : View.GONE);
        } else if (propertyKey == ON_CONFIRM_CLICKED) {
            view.mConfirmButton.setOnClickListener(_ -> model.get(ON_CONFIRM_CLICKED).run());
        } else if (propertyKey == ON_CANCEL_CLICKED) {
            view.mCancelButton.setOnClickListener(_ -> model.get(ON_CANCEL_CLICKED).run());
        } else {
            assert false : "Unhandled property key: " + propertyKey;
        }
    }

    private EmailVerificationBottomSheetViewBinder() {}
}
