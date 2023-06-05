// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors;

import android.util.Pair;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

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

    public static final PropertyModel
            .WritableObjectPropertyKey<List<EditorFieldModel>> EDITOR_FIELDS =
            new PropertyModel.WritableObjectPropertyKey<>("editor_fields");

    public static final PropertyModel.ReadableObjectPropertyKey<Runnable> DONE_RUNNABLE =
            new PropertyModel.ReadableObjectPropertyKey<>("done_callback");
    public static final PropertyModel.ReadableObjectPropertyKey<Runnable> CANCEL_RUNNABLE =
            new PropertyModel.ReadableObjectPropertyKey<>("cancel_callback");

    public static final PropertyKey[] ALL_KEYS = {EDITOR_TITLE, CUSTOM_DONE_BUTTON_TEXT,
            FOOTER_MESSAGE, DELETE_CONFIRMATION_TITLE, DELETE_CONFIRMATION_TEXT,
            SHOW_REQUIRED_INDICATOR, EDITOR_FIELDS, DONE_RUNNABLE, CANCEL_RUNNABLE};

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
        boolean isValid(@Nullable CharSequence value);

        /**
         * Called to check whether the length of the field value is maximum.
         *
         * @param value The value of the field to check.
         * @return True if the field value length is maximum among all the possible valid values in
         *         this field.
         */
        boolean isLengthMaximum(@Nullable CharSequence value);
    }

    /**
     * A convenience class for displaying keyed values in a dropdown.
     */
    public static class DropdownKeyValue extends Pair<String, CharSequence> {
        public DropdownKeyValue(String key, CharSequence value) {
            super(key, value);
        }

        /** @return The key identifier. */
        public String getKey() {
            return super.first;
        }

        /** @return The human-readable localized display value. */
        public CharSequence getValue() {
            return super.second;
        }

        @Override
        public String toString() {
            return super.second.toString();
        }
    }
}
