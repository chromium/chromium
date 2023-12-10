// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors;

import static org.chromium.chrome.browser.autofill.AutofillUiUtils.getInputTypeForField;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.ALLOW_DELETE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.CANCEL_RUNNABLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.CUSTOM_DONE_BUTTON_TEXT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DELETE_CONFIRMATION_TEXT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DELETE_CONFIRMATION_TITLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DELETE_RUNNABLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DONE_RUNNABLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DropdownFieldProperties.DROPDOWN_CALLBACK;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DropdownFieldProperties.DROPDOWN_HINT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DropdownFieldProperties.DROPDOWN_KEY_VALUE_LIST;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.EDITOR_FIELDS;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.EDITOR_TITLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FOOTER_MESSAGE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.ERROR_MESSAGE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.FOCUSED;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.IS_REQUIRED;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.LABEL;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.VALIDATOR;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.VALUE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.SHOW_REQUIRED_INDICATOR;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextFieldProperties.TEXT_FIELD_TYPE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextFieldProperties.TEXT_FORMATTER;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextFieldProperties.TEXT_SUGGESTIONS;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.VALIDATE_ON_SHOW;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.VISIBLE;

import org.chromium.chrome.browser.autofill.editors.EditorProperties.DropdownKeyValue;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;
import java.util.stream.Collectors;

/**
 * Provides functions that map {@link EditorProperties} changes in a {@link PropertyModel} to
 * the suitable method in {@link EditorDialogView}.
 */
public class EditorDialogViewBinder {
    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link EditorDialogView} to update.
     * @param propertyKey The {@link PropertyKey} which changed.
     */
    public static void bindEditorDialogView(
            PropertyModel model, EditorDialogView view, PropertyKey propertyKey) {
        if (propertyKey == EDITOR_TITLE) {
            view.setEditorTitle(model.get(EDITOR_TITLE));
        } else if (propertyKey == CUSTOM_DONE_BUTTON_TEXT) {
            view.setCustomDoneButtonText(model.get(CUSTOM_DONE_BUTTON_TEXT));
        } else if (propertyKey == FOOTER_MESSAGE) {
            view.setFooterMessage(model.get(FOOTER_MESSAGE));
        } else if (propertyKey == DELETE_CONFIRMATION_TITLE) {
            view.setDeleteConfirmationTitle(model.get(DELETE_CONFIRMATION_TITLE));
        } else if (propertyKey == DELETE_CONFIRMATION_TEXT) {
            view.setDeleteConfirmationText(model.get(DELETE_CONFIRMATION_TEXT));
        } else if (propertyKey == SHOW_REQUIRED_INDICATOR) {
            view.setShowRequiredIndicator(model.get(SHOW_REQUIRED_INDICATOR));
        } else if (propertyKey == EDITOR_FIELDS) {
            view.setEditorFields(model.get(EDITOR_FIELDS), model.get(SHOW_REQUIRED_INDICATOR));
        } else if (propertyKey == DONE_RUNNABLE) {
            view.setDoneRunnable(model.get(DONE_RUNNABLE));
        } else if (propertyKey == CANCEL_RUNNABLE) {
            view.setCancelRunnable(model.get(CANCEL_RUNNABLE));
        } else if (propertyKey == ALLOW_DELETE) {
            view.setAllowDelete(model.get(ALLOW_DELETE));
        } else if (propertyKey == DELETE_RUNNABLE) {
            view.setDeleteRunnable(model.get(DELETE_RUNNABLE));
        } else if (propertyKey == VALIDATE_ON_SHOW) {
            view.setValidateOnShow(model.get(VALIDATE_ON_SHOW));
        } else if (propertyKey == VISIBLE) {
            view.setVisible(model.get(VISIBLE));
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

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
            List<String> values =
                    model.get(DROPDOWN_KEY_VALUE_LIST).stream()
                            .map(DropdownKeyValue::getValue)
                            .collect(Collectors.toList());
            view.setDropdownValues(values, model.get(DROPDOWN_HINT));
            view.setValue(model.get(VALUE));
            view.setErrorMessage(model.get(ERROR_MESSAGE));
        } else if (key == DROPDOWN_CALLBACK) {
            // Does not require binding at the moment.
        } else {
            assert false : "Unhandled update to property:" + key;
        }
    }
}
