// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors.common.text_field;

import android.text.TextWatcher;

import com.google.common.collect.ObjectArrays;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.util.List;

/** Properties specific for the text fields. */
@NullMarked
public class TextFieldProperties {
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
            ObjectArrays.concat(
                    FieldProperties.FIELD_ALL_KEYS, TEXT_SPECIFIC_KEYS, PropertyKey.class);

    private TextFieldProperties() {}
}
