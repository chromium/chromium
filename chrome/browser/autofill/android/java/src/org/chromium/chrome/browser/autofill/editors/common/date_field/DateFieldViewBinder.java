// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors.common.date_field;

import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.ERROR_MESSAGE;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.FOCUSED;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.IS_REQUIRED;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.LABEL;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.VALIDATOR;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.VALUE;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** View binder for the {@link DropdownFieldView}. */
@NullMarked
public class DateFieldViewBinder {
    public static void bindDateFieldView(PropertyModel model, DateFieldView view, PropertyKey key) {
        if (key == LABEL || key == IS_REQUIRED) {
            view.setLabel(model.get(LABEL), model.get(IS_REQUIRED));
        } else if (key == VALUE) {
            view.setValue(model.get(VALUE));
        } else if (key == VALIDATOR) {
            // TODO: crbug.com/476755159 - Implement validation.
        } else if (key == ERROR_MESSAGE) {
            view.setErrorMessage(model.get(ERROR_MESSAGE));
        } else if (key == FOCUSED) {
            // TODO: crbug.com/476755159 - Implement focusability.
        } else {
            assert false : "Unhandled update to property:" + key;
        }
    }

    private DateFieldViewBinder() {}
}
