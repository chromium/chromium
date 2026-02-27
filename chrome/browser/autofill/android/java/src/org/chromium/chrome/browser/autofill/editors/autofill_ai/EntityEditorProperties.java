// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors.autofill_ai;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.EditorItem;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;

/** Properties defined here reflect the visible state of the {@link EntityEditorView}. */
@NullMarked
public class EntityEditorProperties {
    public static final ReadableObjectPropertyKey<String> EDITOR_TITLE =
            new ReadableObjectPropertyKey<>("editor_title");

    public static final WritableBooleanPropertyKey VISIBLE =
            new WritableBooleanPropertyKey("visible");

    public static final ReadableObjectPropertyKey<Runnable> DONE_RUNNABLE =
            new ReadableObjectPropertyKey<>("done_callback");
    public static final ReadableObjectPropertyKey<Runnable> CANCEL_RUNNABLE =
            new ReadableObjectPropertyKey<>("cancel_callback");

    public static final ReadableObjectPropertyKey<String> DELETE_CONFIRMATION_TITLE =
            new ReadableObjectPropertyKey<>("delete_confirmation_title");
    public static final ReadableObjectPropertyKey<CharSequence> DELETE_CONFIRMATION_TEXT =
            new ReadableObjectPropertyKey<>("delete_confirmation_text");
    public static final ReadableIntPropertyKey DELETE_CONFIRMATION_PRIMARY_BUTTON_TEXT_ID =
            new ReadableIntPropertyKey("delete_confirmation_primary_button_text");
    public static final ReadableBooleanPropertyKey ALLOW_DELETE =
            new ReadableBooleanPropertyKey("allow_delete");
    public static final ReadableObjectPropertyKey<Callback<Boolean>> DELETE_CALLBACK =
            new ReadableObjectPropertyKey<>("delete_callback");

    public static final ReadableObjectPropertyKey<ListModel<EditorItem>> EDITOR_FIELDS =
            new ReadableObjectPropertyKey<>("editor_fields");

    public static final PropertyKey[] ALL_KEYS = {
        EDITOR_TITLE,
        VISIBLE,
        DONE_RUNNABLE,
        CANCEL_RUNNABLE,
        DELETE_CONFIRMATION_TITLE,
        DELETE_CONFIRMATION_TEXT,
        DELETE_CONFIRMATION_PRIMARY_BUTTON_TEXT_ID,
        ALLOW_DELETE,
        DELETE_CALLBACK,
        EDITOR_FIELDS,
    };

    private EntityEditorProperties() {}
}
