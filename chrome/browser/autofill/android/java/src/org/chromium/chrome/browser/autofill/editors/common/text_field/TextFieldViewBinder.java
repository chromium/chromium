// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors.common.text_field;

import static org.chromium.chrome.browser.autofill.AutofillUiUtils.getInputTypeForField;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.ERROR_MESSAGE;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.FOCUSED;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.IS_REQUIRED;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.LABEL;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.VALIDATOR;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.VALUE;
import static org.chromium.chrome.browser.autofill.editors.common.text_field.TextFieldProperties.TEXT_FIELD_TYPE;
import static org.chromium.chrome.browser.autofill.editors.common.text_field.TextFieldProperties.TEXT_FORMATTER;
import static org.chromium.chrome.browser.autofill.editors.common.text_field.TextFieldProperties.TEXT_SUGGESTIONS;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

@NullMarked
public class TextFieldViewBinder {
    public static void bindTextFieldView(PropertyModel model, TextFieldView view, PropertyKey key) {
        if (key == LABEL || key == IS_REQUIRED) {
            view.setLabel(model.get(LABEL), model.get(IS_REQUIRED));
        } else if (key == VALIDATOR) {
            view.setValidator(model.get(VALIDATOR));
        } else if (key == ERROR_MESSAGE) {
            view.setErrorMessage(model.get(ERROR_MESSAGE));
        } else if (key == FOCUSED) {
            if (model.get(FOCUSED)) {
                view.scrollToAndFocus();
            }
        } else if (key == VALUE) {
            view.setValue(model.get(VALUE));
        } else if (key == TEXT_FIELD_TYPE) {
            // Setting text input triggers TextWatcher, which overwrites the
            // model's text value. Always set value before the text input type
            // to avoid loosing the text value.
            view.setValue(model.get(VALUE));
            view.setTextInputType(getInputTypeForField(model.get(TEXT_FIELD_TYPE)));
        } else if (key == TEXT_SUGGESTIONS) {
            view.setTextSuggestions(model.get(TEXT_SUGGESTIONS));
        } else if (key == TEXT_FORMATTER) {
            view.setTextFormatter(model.get(TEXT_FORMATTER));
        } else {
            assert false : "Unhandled update to property:" + key;
        }
    }

    private TextFieldViewBinder() {}
}
