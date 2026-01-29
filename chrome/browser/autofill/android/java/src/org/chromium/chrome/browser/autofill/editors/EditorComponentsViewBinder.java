// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors;

import static org.chromium.chrome.browser.autofill.AutofillUiUtils.getInputTypeForField;
import static org.chromium.chrome.browser.autofill.editors.EditorComponentsProperties.DropdownFieldProperties.DROPDOWN_CALLBACK;
import static org.chromium.chrome.browser.autofill.editors.EditorComponentsProperties.DropdownFieldProperties.DROPDOWN_HINT;
import static org.chromium.chrome.browser.autofill.editors.EditorComponentsProperties.DropdownFieldProperties.DROPDOWN_KEY_VALUE_LIST;
import static org.chromium.chrome.browser.autofill.editors.EditorComponentsProperties.FieldProperties.ERROR_MESSAGE;
import static org.chromium.chrome.browser.autofill.editors.EditorComponentsProperties.FieldProperties.FOCUSED;
import static org.chromium.chrome.browser.autofill.editors.EditorComponentsProperties.FieldProperties.IS_REQUIRED;
import static org.chromium.chrome.browser.autofill.editors.EditorComponentsProperties.FieldProperties.LABEL;
import static org.chromium.chrome.browser.autofill.editors.EditorComponentsProperties.FieldProperties.VALIDATOR;
import static org.chromium.chrome.browser.autofill.editors.EditorComponentsProperties.FieldProperties.VALUE;
import static org.chromium.chrome.browser.autofill.editors.EditorComponentsProperties.NonEditableTextProperties.CLICK_RUNNABLE;
import static org.chromium.chrome.browser.autofill.editors.EditorComponentsProperties.NonEditableTextProperties.CONTENT_DESCRIPTION;
import static org.chromium.chrome.browser.autofill.editors.EditorComponentsProperties.NonEditableTextProperties.ICON;
import static org.chromium.chrome.browser.autofill.editors.EditorComponentsProperties.NoticeProperties.IMPORTANT_FOR_ACCESSIBILITY;
import static org.chromium.chrome.browser.autofill.editors.EditorComponentsProperties.NoticeProperties.NOTICE_TEXT;
import static org.chromium.chrome.browser.autofill.editors.EditorComponentsProperties.TextFieldProperties.TEXT_FIELD_TYPE;
import static org.chromium.chrome.browser.autofill.editors.EditorComponentsProperties.TextFieldProperties.TEXT_FORMATTER;
import static org.chromium.chrome.browser.autofill.editors.EditorComponentsProperties.TextFieldProperties.TEXT_SUGGESTIONS;

import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.autofill.R;
import org.chromium.components.autofill.DropdownKeyValue;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/**
 * Provides functions that map {@link EditorComponentsProperties} changes in a {@link PropertyModel}
 * to the suitable method in the corresponding views.
 */
@NullMarked
public class EditorComponentsViewBinder {
    static void bindTextFieldView(PropertyModel model, TextFieldView view, PropertyKey key) {
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

    static void bindDropdownFieldView(
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

    static void bindNonEditableTextView(PropertyModel model, View view, PropertyKey key) {
        if (key == EditorComponentsProperties.NonEditableTextProperties.PRIMARY_TEXT) {
            TextView textView = view.findViewById(R.id.primary_text);
            textView.setText(
                    model.get(EditorComponentsProperties.NonEditableTextProperties.PRIMARY_TEXT));
        } else if (key == EditorComponentsProperties.NonEditableTextProperties.SECONDARY_TEXT) {
            TextView secondaryTextView = view.findViewById(R.id.secondary_text);
            String secondaryText =
                    model.get(EditorComponentsProperties.NonEditableTextProperties.SECONDARY_TEXT);
            if (secondaryText != null && !secondaryText.isEmpty()) {
                secondaryTextView.setText(secondaryText);
                secondaryTextView.setVisibility(View.VISIBLE);
            } else {
                secondaryTextView.setVisibility(View.GONE);
            }
        } else if (key == ICON) {
            ImageView iconView = view.findViewById(R.id.icon);
            iconView.setImageResource(model.get(ICON));
            iconView.setVisibility(View.VISIBLE);
        } else if (key == CLICK_RUNNABLE) {
            view.setOnClickListener(v -> model.get(CLICK_RUNNABLE).run());
        } else if (key == CONTENT_DESCRIPTION) {
            view.setContentDescription(model.get(CONTENT_DESCRIPTION));
        } else {
            assert false : "Unhandled update to property:" + key;
        }
    }

    static void bindNoticeTextView(PropertyModel model, TextView view, PropertyKey key) {
        if (key == NOTICE_TEXT) {
            view.setText(model.get(NOTICE_TEXT));
        } else if (key == IMPORTANT_FOR_ACCESSIBILITY) {
            view.setImportantForAccessibility(
                    model.get(IMPORTANT_FOR_ACCESSIBILITY)
                            ? View.IMPORTANT_FOR_ACCESSIBILITY_YES
                            : View.IMPORTANT_FOR_ACCESSIBILITY_NO);
        } else {
            assert false : "Unhandled update to property:" + key;
        }
    }
}
