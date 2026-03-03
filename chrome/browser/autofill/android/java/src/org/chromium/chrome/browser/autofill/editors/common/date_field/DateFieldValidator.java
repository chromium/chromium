// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors.common.date_field;

import static org.chromium.chrome.browser.autofill.editors.common.date_field.DateFieldProperties.DATE_VALID;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.ERROR_MESSAGE;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.autofill.editors.common.field.EditorFieldValidator;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * A special instance of the {@link EditorFieldValidator} that deals with the invalid states of the
 * {@link DateFieldView}. Unlike {@link TextFieldView} and {@link DropdownFieldView}, the date
 * picker can be in the invalid state, which is not reflected in the `VALUE` property of the field.
 * This validator sets the corresponding error message if the `DATE_VALID` property is `false`.
 */
@NullMarked
public class DateFieldValidator extends EditorFieldValidator {
    private final String mInvalidFieldErrorMessage;

    public DateFieldValidator(String invalidFieldErrorMessage) {
        mInvalidFieldErrorMessage = invalidFieldErrorMessage;
    }

    @Override
    public void validate(PropertyModel fieldModel) {
        super.validate(fieldModel);
        if (fieldModel.get(ERROR_MESSAGE) != null) {
            // The date field already didn't pass the validation.
            return;
        }
        if (!fieldModel.get(DATE_VALID)) {
            fieldModel.set(ERROR_MESSAGE, mInvalidFieldErrorMessage);
            return;
        }
        fieldModel.set(ERROR_MESSAGE, null);
    }
}
