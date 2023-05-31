// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.prefeditor;

import org.chromium.chrome.browser.autofill.editors.EditorFieldModel;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

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
}
