// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors.common.field;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Field properties common to every field. */
@NullMarked
public class FieldProperties {
    public static final WritableObjectPropertyKey<String> LABEL =
            new WritableObjectPropertyKey<>("label");
    public static final PropertyModel.WritableObjectPropertyKey<EditorFieldValidator> VALIDATOR =
            new WritableObjectPropertyKey<>("validator");
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

    private FieldProperties() {}
}
