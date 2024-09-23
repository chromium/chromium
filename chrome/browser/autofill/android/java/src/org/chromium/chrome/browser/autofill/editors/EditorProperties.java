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
import org.chromium.ui.modelutil.PropertyModel.ReadableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Arrays;
import java.util.List;
import java.util.stream.Stream;

/** Properties defined here reflect the visible state of the {@link EditorDialog}. */
public class EditorProperties {
    /** Contains information needed by {@link EditorDialogView} to display fields. */
    public static class FieldItem extends ListItem {
        public final boolean isFullLine;

        public FieldItem(int type, PropertyModel model) {
            this(type, model, /* isFullLine= */ false);
        }

        public FieldItem(int type, PropertyModel model, boolean isFullLine) {
            super(type, model);
            this.isFullLine = isFullLine;
        }
    }

    public static final ReadableObjectPropertyKey<String> EDITOR_TITLE =
            new ReadableObjectPropertyKey<>("editor_title");
    public static final ReadableObjectPropertyKey<String> CUSTOM_DONE_BUTTON_TEXT =
            new ReadableObjectPropertyKey<String>("custom_done_button_text");
    public static final ReadableObjectPropertyKey<String> FOOTER_MESSAGE =
            new ReadableObjectPropertyKey<>("footer_message");
    public static final ReadableObjectPropertyKey<String> DELETE_CONFIRMATION_TITLE =
            new ReadableObjectPropertyKey<>("delete_confirmation_title");
    public static final ReadableObjectPropertyKey<String> DELETE_CONFIRMATION_TEXT =
            new ReadableObjectPropertyKey<>("delete_confirmation_text");
    public static final ReadableBooleanPropertyKey SHOW_REQUIRED_INDICATOR =
            new ReadableBooleanPropertyKey("show_required_indicator");

    public static final WritableObjectPropertyKey<ListModel<FieldItem>> EDITOR_FIELDS =
            new WritableObjectPropertyKey<>("editor_fields");

    public static final ReadableObjectPropertyKey<Runnable> DONE_RUNNABLE =
            new ReadableObjectPropertyKey<>("done_callback");
    public static final ReadableObjectPropertyKey<Runnable> CANCEL_RUNNABLE =
            new ReadableObjectPropertyKey<>("cancel_callback");

    public static final ReadableBooleanPropertyKey ALLOW_DELETE =
            new ReadableBooleanPropertyKey("allow_delete");
    public static final ReadableObjectPropertyKey<Runnable> DELETE_RUNNABLE =
            new ReadableObjectPropertyKey<>("delete_callback");

    public static final WritableBooleanPropertyKey VALIDATE_ON_SHOW =
            new WritableBooleanPropertyKey("validate_on_show");

    public static final WritableBooleanPropertyKey VISIBLE =
            new WritableBooleanPropertyKey("visible");

    public static final PropertyKey[] ALL_KEYS = {
        EDITOR_TITLE,
        CUSTOM_DONE_BUTTON_TEXT,
        FOOTER_MESSAGE,
        DELETE_CONFIRMATION_TITLE,
        DELETE_CONFIRMATION_TEXT,
        SHOW_REQUIRED_INDICATOR,
        EDITOR_FIELDS,
        DONE_RUNNABLE,
        CANCEL_RUNNABLE,
        ALLOW_DELETE,
        DELETE_RUNNABLE,
        VALIDATE_ON_SHOW,
        VISIBLE
    };

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

    /** A convenience class for displaying keyed values in a dropdown. */
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

    /** Field properties common to every field. */
    public static class FieldProperties {
        public static final WritableObjectPropertyKey<String> LABEL =
                new WritableObjectPropertyKey<>("label");
        public static final PropertyModel.WritableObjectPropertyKey<EditorFieldValidator>
                VALIDATOR = new WritableObjectPropertyKey<>("validator");
        public static final WritableObjectPropertyKey<String> ERROR_MESSAGE =
                new WritableObjectPropertyKey<>("error_message");
        // TODO(crbug.com/40265078): make this field read-only.
        public static final WritableBooleanPropertyKey IS_REQUIRED =
                new WritableBooleanPropertyKey("is_required");
        public static final WritableBooleanPropertyKey FOCUSED =
                new WritableBooleanPropertyKey("focused");
        // TODO(crbug.com/40265078): make this field read-only.
        public static final WritableObjectPropertyKey<String> VALUE =
                new WritableObjectPropertyKey<>("value");

        public static final PropertyKey[] FIELD_ALL_KEYS = {
            LABEL, VALIDATOR, IS_REQUIRED, ERROR_MESSAGE, FOCUSED, VALUE
        };
    }

    /** Properties specific for the dropdown fields. */
    public static class DropdownFieldProperties {
        public static final ReadableObjectPropertyKey<List<DropdownKeyValue>>
                DROPDOWN_KEY_VALUE_LIST = new ReadableObjectPropertyKey<>("key_value_list");
        public static final WritableObjectPropertyKey<Callback<String>> DROPDOWN_CALLBACK =
                new WritableObjectPropertyKey<>("callback");
        public static final ReadableObjectPropertyKey<String> DROPDOWN_HINT =
                new ReadableObjectPropertyKey<>("hint");

        public static final PropertyKey[] DROPDOWN_SPECIFIC_KEYS = {
            DROPDOWN_KEY_VALUE_LIST, DROPDOWN_CALLBACK, DROPDOWN_HINT
        };

        public static final PropertyKey[] DROPDOWN_ALL_KEYS =
                Stream.concat(
                                Arrays.stream(FieldProperties.FIELD_ALL_KEYS),
                                Arrays.stream(DROPDOWN_SPECIFIC_KEYS))
                        .toArray(PropertyKey[]::new);
    }

    /** Properties specific for the text fields. */
    public static class TextFieldProperties {
        public static final ReadableIntPropertyKey TEXT_FIELD_TYPE =
                new ReadableIntPropertyKey("field_type");
        public static final WritableObjectPropertyKey<List<String>> TEXT_SUGGESTIONS =
                new WritableObjectPropertyKey<>("suggestions");
        public static final ReadableObjectPropertyKey<TextWatcher> TEXT_FORMATTER =
                new ReadableObjectPropertyKey<>("formatter");

        public static final PropertyKey[] TEXT_SPECIFIC_KEYS = {
            TEXT_FIELD_TYPE, TEXT_SUGGESTIONS, TEXT_FORMATTER,
        };

        public static final PropertyKey[] TEXT_ALL_KEYS =
                Stream.concat(
                                Arrays.stream(FieldProperties.FIELD_ALL_KEYS),
                                Arrays.stream(TEXT_SPECIFIC_KEYS))
                        .toArray(PropertyKey[]::new);
    }

    public static boolean isDropdownField(ListItem fieldItem) {
        return fieldItem.type == ItemType.DROPDOWN;
    }

    public static @Nullable String getDropdownKeyByValue(
            PropertyModel dropdownField, String value) {
        return dropdownField.get(DropdownFieldProperties.DROPDOWN_KEY_VALUE_LIST).stream()
                .filter(
                        keyValue -> {
                            return keyValue.getValue().equals(value);
                        })
                .map(DropdownKeyValue::getKey)
                .findAny()
                .orElse(null);
    }

    public static @Nullable String getDropdownValueByKey(PropertyModel dropdownField, String key) {
        return dropdownField.get(DropdownFieldProperties.DROPDOWN_KEY_VALUE_LIST).stream()
                .filter(
                        keyValue -> {
                            return keyValue.getKey().equals(key);
                        })
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

    public static void scrollToFieldWithErrorMessage(PropertyModel editorModel) {
        // Check if a field with an error is already focused.
        ListModel<FieldItem> fields = editorModel.get(EditorProperties.EDITOR_FIELDS);
        for (FieldItem item : fields) {
            if (item.model.get(FieldProperties.FOCUSED)
                    && item.model.get(FieldProperties.ERROR_MESSAGE) != null) {
                // Hack: Although the field is focused, it may be off screen. Toggle FOCUSED in
                // order to scroll the field into view.
                item.model.set(FieldProperties.FOCUSED, false);
                item.model.set(FieldProperties.FOCUSED, true);
                return;
            }
        }

        // Focus first field with an error.
        for (FieldItem item : fields) {
            if (item.model.get(FieldProperties.ERROR_MESSAGE) != null) {
                item.model.set(FieldProperties.FOCUSED, true);
                break;
            }
            // The field (ex {@link TextFieldView}) is responsible for clearing FOCUSED property
            // when the field loses focus.
        }
    }
}
