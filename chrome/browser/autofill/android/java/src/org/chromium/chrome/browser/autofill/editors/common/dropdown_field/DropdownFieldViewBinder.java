// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors.common.dropdown_field;

import static org.chromium.chrome.browser.autofill.editors.common.dropdown_field.DropdownFieldProperties.DROPDOWN_CALLBACK;
import static org.chromium.chrome.browser.autofill.editors.common.dropdown_field.DropdownFieldProperties.DROPDOWN_HINT;
import static org.chromium.chrome.browser.autofill.editors.common.dropdown_field.DropdownFieldProperties.DROPDOWN_KEY_VALUE_LIST;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.ERROR_MESSAGE;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.FOCUSED;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.IS_REQUIRED;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.LABEL;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.VALIDATOR;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.VALUE;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.autofill.DropdownKeyValue;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/** View binder for the {@link DropdownFieldView}. */
@NullMarked
public class DropdownFieldViewBinder {
    public static void bindDropdownFieldView(
            PropertyModel model, DropdownFieldView view, PropertyKey key) {
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
        } else if (key == DROPDOWN_KEY_VALUE_LIST || key == DROPDOWN_HINT) {
            List<String> values = new ArrayList<>();
            for (DropdownKeyValue keyValue : model.get(DROPDOWN_KEY_VALUE_LIST)) {
                values.add(keyValue.getValue());
            }
            view.setDropdownValues(values, model.get(DROPDOWN_HINT));
            view.setValue(model.get(VALUE));
            view.setErrorMessage(model.get(ERROR_MESSAGE));
        } else if (key == DROPDOWN_CALLBACK) {
            // Does not require binding at the moment.
        } else {
            assert false : "Unhandled update to property:" + key;
        }
    }

    private DropdownFieldViewBinder() {}
}
