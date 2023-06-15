// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors;

import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DropdownFieldProperties.DROPDOWN_HINT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DropdownFieldProperties.DROPDOWN_KEY_VALUE_LIST;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.CUSTOM_ERROR_MESSAGE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.INVALID_ERROR_MESSAGE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.IS_REQUIRED;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.LABEL;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.REQUIRED_ERROR_MESSAGE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.VALIDATOR;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.VALUE;

import android.text.TextUtils;
import android.text.TextWatcher;
import android.util.Pair;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Arrays;
import java.util.List;
import java.util.stream.Stream;

/**
 * Properties defined here reflect the visible state of the {@link EditorDialog}.
 */
public class EditorProperties {
    public static final PropertyModel.ReadableObjectPropertyKey<String> EDITOR_TITLE =
            new PropertyModel.ReadableObjectPropertyKey<>("editor_title");
    public static final PropertyModel.ReadableObjectPropertyKey<String> CUSTOM_DONE_BUTTON_TEXT =
            new PropertyModel.ReadableObjectPropertyKey<String>("custom_done_button_text");
    public static final PropertyModel.ReadableObjectPropertyKey<String> FOOTER_MESSAGE =
            new PropertyModel.ReadableObjectPropertyKey<>("footer_message");
    public static final PropertyModel.ReadableObjectPropertyKey<String> DELETE_CONFIRMATION_TITLE =
            new PropertyModel.ReadableObjectPropertyKey<>("delete_confirmation_title");
    public static final PropertyModel.ReadableObjectPropertyKey<String> DELETE_CONFIRMATION_TEXT =
            new PropertyModel.ReadableObjectPropertyKey<>("delete_confirmation_text");
    public static final PropertyModel.ReadableBooleanPropertyKey SHOW_REQUIRED_INDICATOR =
            new PropertyModel.ReadableBooleanPropertyKey("show_required_indicator");
    /**
     * If true, done callback is triggered immediately after the user clicked
     * on the done button. Otherwise, by default, it is triggered only after the dialog is
     * dismissed with animation.
     */
    public static final PropertyModel
            .ReadableBooleanPropertyKey TRIGGER_DONE_CALLBACK_BEFORE_CLOSE_ANIMATION =
            new PropertyModel.ReadableBooleanPropertyKey(
                    "trigger_done_callback_before_close_animation");

    public static final PropertyModel.WritableObjectPropertyKey<ListModel<ListItem>> EDITOR_FIELDS =
            new PropertyModel.WritableObjectPropertyKey<>("editor_fields");

    public static final PropertyModel.ReadableObjectPropertyKey<Runnable> DONE_RUNNABLE =
            new PropertyModel.ReadableObjectPropertyKey<>("done_callback");
    public static final PropertyModel.ReadableObjectPropertyKey<Runnable> CANCEL_RUNNABLE =
            new PropertyModel.ReadableObjectPropertyKey<>("cancel_callback");

    public static final PropertyModel.ReadableBooleanPropertyKey ALLOW_DELETE =
            new PropertyModel.ReadableBooleanPropertyKey("allow_delete");
    public static final PropertyModel.ReadableObjectPropertyKey<Runnable> DELETE_RUNNABLE =
            new PropertyModel.ReadableObjectPropertyKey<>("delete_callback");

    public static final PropertyKey[] ALL_KEYS = {EDITOR_TITLE, CUSTOM_DONE_BUTTON_TEXT,
            FOOTER_MESSAGE, DELETE_CONFIRMATION_TITLE, DELETE_CONFIRMATION_TEXT,
            SHOW_REQUIRED_INDICATOR, TRIGGER_DONE_CALLBACK_BEFORE_CLOSE_ANIMATION, EDITOR_FIELDS,
            DONE_RUNNABLE, CANCEL_RUNNABLE, ALLOW_DELETE, DELETE_RUNNABLE};

    private EditorProperties() {}

    /*
     * Types of fields this editor model supports.
     */
    @IntDef({ItemType.DROPDOWN, ItemType.TEXT_INPUT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ItemType {
        // A fixed list of values, only 1 of which can be selected.
        int DROPDOWN = 1;
        // User can fill in a sequence of characters subject to input type restrictions.
        int TEXT_INPUT = 2;
    }

    @IntDef({
            TextInputType.PLAIN_TEXT_INPUT,
            TextInputType.PHONE_NUMBER_INPUT,
            TextInputType.EMAIL_ADDRESS_INPUT,
            TextInputType.STREET_ADDRESS_INPUT,
            TextInputType.PERSON_NAME_INPUT,
            TextInputType.ALPHA_NUMERIC_INPUT,
            TextInputType.REGION_INPUT,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface TextInputType {
        // All symbols are allowed.
        int PLAIN_TEXT_INPUT = 1;
        // Only numbers and phone related symbols should be entered.
        int PHONE_NUMBER_INPUT = 2;
        // Text with email address symbols should be entered.
        int EMAIL_ADDRESS_INPUT = 3;
        // Text with numbers should be entered.
        int STREET_ADDRESS_INPUT = 4;
        // Only text symbols should be entered.
        int PERSON_NAME_INPUT = 5;
        // Text symbols and number should be entered.
        int ALPHA_NUMERIC_INPUT = 6;
        // All letters will be capitalized.
        int REGION_INPUT = 7;
    }

    /**
     * The interface to be implemented by the field validator.
     */
    public interface EditorFieldValidator {
        /**
         * Called to check the validity of the field value.
         *
         * @param value The value of the field to check.
         * @return True if the value is valid.
         */
        boolean isValid(@Nullable String value);

        /**
         * Called to check whether the length of the field value is maximum.
         *
         * @param value The value of the field to check.
         * @return True if the field value length is maximum among all the possible valid values in
         *         this field.
         */
        boolean isLengthMaximum(@Nullable String value);
    }

    /**
     * A convenience class for displaying keyed values in a dropdown.
     */
    public static class DropdownKeyValue extends Pair<String, String> {
        public DropdownKeyValue(String key, String value) {
            super(key, value);
        }

        /** @return The key identifier. */
        public String getKey() {
            return super.first;
        }

        /** @return The human-readable localized display value. */
        public String getValue() {
            return super.second;
        }

        @Override
        public String toString() {
            return super.second.toString();
        }
    }

    /**
     * Field properties common to every field.
     */
    public static class FieldProperties {
        public static final PropertyModel.WritableObjectPropertyKey<String> LABEL =
                new PropertyModel.WritableObjectPropertyKey<>("label");
        public static final PropertyModel
                .ReadableObjectPropertyKey<EditorFieldValidator> VALIDATOR =
                new PropertyModel.ReadableObjectPropertyKey<>("validator");
        // TODO(crbug.com/1435314): make this field read-only.
        public static final PropertyModel.WritableBooleanPropertyKey IS_REQUIRED =
                new PropertyModel.WritableBooleanPropertyKey("is_required");
        // TODO(crbug.com/1435314): make this field read-only.
        public static final PropertyModel.WritableObjectPropertyKey<String> REQUIRED_ERROR_MESSAGE =
                new PropertyModel.WritableObjectPropertyKey<>("required_error_message");
        public static final PropertyModel.ReadableObjectPropertyKey<String> INVALID_ERROR_MESSAGE =
                new PropertyModel.ReadableObjectPropertyKey<>("invalid_error_message");
        public static final PropertyModel.WritableObjectPropertyKey<String> CUSTOM_ERROR_MESSAGE =
                new PropertyModel.WritableObjectPropertyKey<>("custom_error_message");
        public static final PropertyModel.WritableBooleanPropertyKey IS_FULL_LINE =
                new PropertyModel.WritableBooleanPropertyKey("is_full_line");
        public static final PropertyModel.WritableObjectPropertyKey<String> VALUE =
                new PropertyModel.WritableObjectPropertyKey<>("value");

        public static final PropertyKey[] FIELD_ALL_KEYS = {LABEL, VALIDATOR, IS_REQUIRED,
                REQUIRED_ERROR_MESSAGE, INVALID_ERROR_MESSAGE, CUSTOM_ERROR_MESSAGE, IS_FULL_LINE,
                VALUE};
    }

    /**
     * Properties specific for the dropdown fields.
     */
    public static class DropdownFieldProperties {
        public static final PropertyModel
                .ReadableObjectPropertyKey<List<DropdownKeyValue>> DROPDOWN_KEY_VALUE_LIST =
                new PropertyModel.ReadableObjectPropertyKey<>("key_value_list");
        public static final PropertyModel
                .WritableObjectPropertyKey<Callback<String>> DROPDOWN_CALLBACK =
                new PropertyModel.WritableObjectPropertyKey<>("callback");
        public static final PropertyModel.ReadableObjectPropertyKey<String> DROPDOWN_HINT =
                new PropertyModel.ReadableObjectPropertyKey<>("hint");

        public static final PropertyKey[] DROPDOWN_SPECIFIC_KEYS = {
                DROPDOWN_KEY_VALUE_LIST, DROPDOWN_CALLBACK, DROPDOWN_HINT};

        public static final PropertyKey[] DROPDOWN_ALL_KEYS =
                Stream.concat(Arrays.stream(FieldProperties.FIELD_ALL_KEYS),
                              Arrays.stream(DROPDOWN_SPECIFIC_KEYS))
                        .toArray(PropertyKey[] ::new);
    }

    /**
     * Properties specific for the text fields.
     */
    public static class TextFieldProperties {
        /* Indicates that the length counter is disabled. */
        public static final int LENGTH_COUNTER_LIMIT_NONE = 0;

        public static final PropertyModel.ReadableIntPropertyKey TEXT_INPUT_TYPE =
                new PropertyModel.ReadableIntPropertyKey("text_input_type");
        public static final PropertyModel.WritableObjectPropertyKey<List<String>> TEXT_SUGGESTIONS =
                new PropertyModel.WritableObjectPropertyKey<>("suggestions");
        public static final PropertyModel.ReadableObjectPropertyKey<TextWatcher> TEXT_FORMATTER =
                new PropertyModel.ReadableObjectPropertyKey<>("formatter");
        public static final PropertyModel.ReadableIntPropertyKey TEXT_LENGTH_COUNTER_LIMIT =
                new PropertyModel.ReadableIntPropertyKey("length_counter_limit");

        public static final PropertyKey[] TEXT_SPECIFIC_KEYS = {
                TEXT_INPUT_TYPE,
                TEXT_SUGGESTIONS,
                TEXT_FORMATTER,
                TEXT_LENGTH_COUNTER_LIMIT,
        };

        public static final PropertyKey[] TEXT_ALL_KEYS =
                Stream.concat(Arrays.stream(FieldProperties.FIELD_ALL_KEYS),
                              Arrays.stream(TEXT_SPECIFIC_KEYS))
                        .toArray(PropertyKey[] ::new);
    }

    public static boolean isDropdownField(ListItem fieldItem) {
        return fieldItem.type == ItemType.DROPDOWN;
    }

    public static @Nullable String getDropdownKeyByValue(
            PropertyModel dropdownField, String value) {
        return dropdownField.get(DropdownFieldProperties.DROPDOWN_KEY_VALUE_LIST)
                .stream()
                .filter(keyValue -> { return keyValue.getValue().equals(value); })
                .map(DropdownKeyValue::getKey)
                .findAny()
                .orElse(null);
    }

    public static @Nullable String getDropdownValueByKey(PropertyModel dropdownField, String key) {
        return dropdownField.get(DropdownFieldProperties.DROPDOWN_KEY_VALUE_LIST)
                .stream()
                .filter(keyValue -> { return keyValue.getKey().equals(key); })
                .map(DropdownKeyValue::getValue)
                .findAny()
                .orElse(null);
    }

    public static void setDropdownKey(PropertyModel dropdownField, @Nullable String key) {
        // The mValue can only be set to null if there is a hint.
        if (key == null && dropdownField.get(DropdownFieldProperties.DROPDOWN_HINT) == null) {
            return;
        }
        dropdownField.set(FieldProperties.VALUE, key);
        Callback<String> fieldCallback =
                dropdownField.get(DropdownFieldProperties.DROPDOWN_CALLBACK);
        if (fieldCallback != null) {
            fieldCallback.onResult(key);
        }
    }

    public static boolean hasMaximumLength(PropertyModel textField) {
        EditorFieldValidator validator = textField.get(FieldProperties.VALIDATOR);
        return validator != null && validator.isLengthMaximum(textField.get(FieldProperties.VALUE));
    }

    public static @Nullable String getValidationErrorMessage(PropertyModel textField) {
        final String customErrorMessage = textField.get(FieldProperties.CUSTOM_ERROR_MESSAGE);
        if (!TextUtils.isEmpty(customErrorMessage)) {
            return customErrorMessage;
        }

        final String value = textField.get(FieldProperties.VALUE);
        if (textField.get(FieldProperties.IS_REQUIRED)
                && (TextUtils.isEmpty(value) || TextUtils.getTrimmedLength(value) == 0)) {
            return textField.get(FieldProperties.REQUIRED_ERROR_MESSAGE);
        }

        final @Nullable EditorFieldValidator validator = textField.get(FieldProperties.VALIDATOR);
        if (validator != null && !validator.isValid(value)) {
            return textField.get(FieldProperties.INVALID_ERROR_MESSAGE);
        }

        return null;
    }

    public static boolean isFieldValid(PropertyModel textField) {
        return getValidationErrorMessage(textField) == null;
    }
}
