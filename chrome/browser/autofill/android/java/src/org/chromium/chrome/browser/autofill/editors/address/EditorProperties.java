// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors.address;

import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.isEditable;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.ERROR_MESSAGE;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.FOCUSED;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.VALIDATOR;

import android.app.Activity;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.EditorItem;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties defined here reflect the visible state of the {@link EditorDialog}. */
@NullMarked
public class EditorProperties {
    public static final ReadableObjectPropertyKey<String> EDITOR_TITLE =
            new ReadableObjectPropertyKey<>("editor_title");
    public static final ReadableObjectPropertyKey<String> CUSTOM_DONE_BUTTON_TEXT =
            new ReadableObjectPropertyKey<>("custom_done_button_text");
    public static final ReadableObjectPropertyKey<String> DELETE_CONFIRMATION_TITLE =
            new ReadableObjectPropertyKey<>("delete_confirmation_title");
    public static final ReadableObjectPropertyKey<CharSequence> DELETE_CONFIRMATION_TEXT =
            new ReadableObjectPropertyKey<>("delete_confirmation_text");
    public static final ReadableIntPropertyKey DELETE_CONFIRMATION_PRIMARY_BUTTON_TEXT_ID =
            new ReadableIntPropertyKey("delete_confirmation_primary_button_text");

    public static final WritableObjectPropertyKey<ListModel<EditorItem>> EDITOR_FIELDS =
            new WritableObjectPropertyKey<>("editor_fields");

    public static final ReadableObjectPropertyKey<Callback<Activity>> OPEN_HELP_CALLBACK =
            new ReadableObjectPropertyKey<>("open_help_callback");

    public static final ReadableObjectPropertyKey<Runnable> DONE_RUNNABLE =
            new ReadableObjectPropertyKey<>("done_callback");
    public static final ReadableObjectPropertyKey<Runnable> CANCEL_RUNNABLE =
            new ReadableObjectPropertyKey<>("cancel_callback");

    public static final ReadableBooleanPropertyKey ALLOW_DELETE =
            new ReadableBooleanPropertyKey("allow_delete");
    public static final ReadableObjectPropertyKey<Callback<Boolean>> DELETE_CALLBACK =
            new ReadableObjectPropertyKey<>("delete_callback");

    public static final WritableBooleanPropertyKey VALIDATE_ON_SHOW =
            new WritableBooleanPropertyKey("validate_on_show");

    public static final WritableBooleanPropertyKey VISIBLE =
            new WritableBooleanPropertyKey("visible");

    public static final WritableBooleanPropertyKey SHOW_BUTTONS =
            new WritableBooleanPropertyKey("show_buttons");

    public static final PropertyKey[] ALL_KEYS = {
        EDITOR_TITLE,
        CUSTOM_DONE_BUTTON_TEXT,
        DELETE_CONFIRMATION_TITLE,
        DELETE_CONFIRMATION_TEXT,
        DELETE_CONFIRMATION_PRIMARY_BUTTON_TEXT_ID,
        EDITOR_FIELDS,
        OPEN_HELP_CALLBACK,
        DONE_RUNNABLE,
        CANCEL_RUNNABLE,
        ALLOW_DELETE,
        DELETE_CALLBACK,
        VALIDATE_ON_SHOW,
        VISIBLE,
        SHOW_BUTTONS
    };

    private EditorProperties() {}

    public static boolean validateForm(PropertyModel editorModel) {
        boolean isValid = true;
        for (ListItem item : editorModel.get(EditorProperties.EDITOR_FIELDS)) {
            if (!isEditable(item)) {
                continue;
            }
            if (item.model.get(VALIDATOR) == null) {
                continue;
            }
            item.model.get(VALIDATOR).validate(item.model);
            isValid &= item.model.get(ERROR_MESSAGE) == null;
        }
        return isValid;
    }

    public static void scrollToFieldWithErrorMessage(PropertyModel editorModel) {
        // Check if a field with an error is already focused.
        ListModel<EditorItem> fields = editorModel.get(EditorProperties.EDITOR_FIELDS);
        for (EditorItem item : fields) {
            if (!isEditable(item)) {
                continue;
            }
            if (item.model.get(FOCUSED) && item.model.get(ERROR_MESSAGE) != null) {
                // Hack: Although the field is focused, it may be off screen. Toggle FOCUSED in
                // order to scroll the field into view.
                item.model.set(FOCUSED, false);
                item.model.set(FOCUSED, true);
                return;
            }
        }

        // Focus first field with an error.
        for (EditorItem item : fields) {
            if (!isEditable(item)) {
                continue;
            }
            if (item.model.get(ERROR_MESSAGE) != null) {
                item.model.set(FOCUSED, true);
                break;
            }
            // The field (ex {@link TextFieldView}) is responsible for clearing FOCUSED property
            // when the field loses focus.
        }
    }
}
