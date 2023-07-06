// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors;

import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DropdownFieldProperties.DROPDOWN_HINT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DropdownFieldProperties.DROPDOWN_KEY_VALUE_LIST;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.IS_REQUIRED;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.LABEL;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.VALIDATOR;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.VALUE;

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
    /**
     * Contains information needed by {@link EditorDialogView} to display fields.
     */
    public static class FieldItem extends ListItem {
        public final boolean isFullLine;

        public FieldItem(int type, PropertyModel model) {
            this(type, model, /*isFullLine=*/false);
        }

        public FieldItem(int type, PropertyModel model, boolean isFullLine) {
            super(type, model);
            this.isFullLine = isFullLine;
        }
    }

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

    public static final PropertyModel
            .WritableObjectPropertyKey<ListModel<FieldItem>> EDITOR_FIELDS =
            new PropertyModel.WritableObjectPropertyKey<>("editor_fields");

    public static final PropertyModel.ReadableObjectPropertyKey<Runnable> DONE_RUNNABLE =
            new PropertyModel.ReadableObjectPropertyKey<>("done_callback");
    public static final PropertyModel.ReadableObjectPropertyKey<Runnable> CANCEL_RUNNABLE =
            new PropertyModel.ReadableObjectPropertyKey<>("cancel_callback");

    public static final PropertyModel.ReadableBooleanPropertyKey ALLOW_DELETE =
            new PropertyModel.ReadableBooleanPropertyKey("allow_delete");
    public static final PropertyModel.ReadableObjectPropertyKey<Runnable> DELETE_RUNNABLE =
            new PropertyModel.ReadableObjectPropertyKey<>("delete_callback");

    public static final PropertyModel.WritableBooleanPropertyKey VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey("visible");
    /**
     * This property is temporary way to trigger field error message update process.
     * It also triggers field focus update.
     * TODO(crbug.com/1435314): remove this property once fields are updated through MCP.
     */
    public static final PropertyModel.WritableBooleanPropertyKey FORM_VALID =
            new PropertyModel.WritableBooleanPropertyKey("form_valid");

    public static final PropertyKey[] ALL_KEYS = {EDITOR_TITLE, CUSTOM_DONE_BUTTON_TEXT,
            FOOTER_MESSAGE, DELETE_CONFIRMATION_TITLE, DELETE_CONFIRMATION_TEXT,
            SHOW_REQUIRED_INDICATOR, EDITOR_FIELDS, DONE_RUNNABLE, CANCEL_RUNNABLE, ALLOW_DELETE,
            DELETE_RUNNABLE, VISIBLE, FORM_VALID};

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
                .WritableObjectPropertyKey<EditorFieldValidator> VALIDATOR =
                new PropertyModel.WritableObjectPropertyKey<>("validator");
        public static final PropertyModel.WritableObjectPropertyKey<String> ERROR_MESSAGE =
                new PropertyModel.WritableObjectPropertyKey<>("error_message");
        // TODO(crbug.com/1435314): make this field read-only.
        public static final PropertyModel.WritableBooleanPropertyKey IS_REQUIRED =
                new PropertyModel.WritableBooleanPropertyKey("is_required");
        // TODO(crbug.com/1435314): make this field read-only.
        public static final PropertyModel.WritableObjectPropertyKey<String> VALUE =
                new PropertyModel.WritableObjectPropertyKey<>("value");

        public static final PropertyKey[] FIELD_ALL_KEYS = {
                LABEL, VALIDATOR, IS_REQUIRED, ERROR_MESSAGE, VALUE};
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
        public static final PropertyModel.ReadableIntPropertyKey TEXT_FIELD_TYPE =
                new PropertyModel.ReadableIntPropertyKey("field_type");
        public static final PropertyModel.WritableObjectPropertyKey<List<String>> TEXT_SUGGESTIONS =
                new PropertyModel.WritableObjectPropertyKey<>("suggestions");
        public static final PropertyModel.ReadableObjectPropertyKey<TextWatcher> TEXT_FORMATTER =
                new PropertyModel.ReadableObjectPropertyKey<>("formatter");

        public static final PropertyKey[] TEXT_SPECIFIC_KEYS = {
                TEXT_FIELD_TYPE,
                TEXT_SUGGESTIONS,
                TEXT_FORMATTER,
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

    public static boolean validateForm(PropertyModel editorModel) {
        boolean isValid = true;
        for (ListItem item : editorModel.get(EditorProperties.EDITOR_FIELDS)) {
            if (item.model.get(FieldProperties.VALIDATOR) == null) {
                continue;
            }
            item.model.get(FieldProperties.VALIDATOR).validate(item.model);
            isValid &= item.model.get(FieldProperties.ERROR_MESSAGE) == null;
        }
        return isValid;
    }
}
