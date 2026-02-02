// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors.common.dropdown_field;

import static org.chromium.build.NullUtil.assumeNonNull;

import com.google.common.collect.ObjectArrays;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties;
import org.chromium.components.autofill.DropdownKeyValue;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.util.List;

/** Properties specific for the dropdown fields. */
@NullMarked
public class DropdownFieldProperties {
    public static final ReadableObjectPropertyKey<List<DropdownKeyValue>> DROPDOWN_KEY_VALUE_LIST =
            new ReadableObjectPropertyKey<>("key_value_list");
    public static final WritableObjectPropertyKey<Callback<String>> DROPDOWN_CALLBACK =
            new WritableObjectPropertyKey<>("callback");
    public static final ReadableObjectPropertyKey<String> DROPDOWN_HINT =
            new ReadableObjectPropertyKey<>("hint");

    public static final PropertyKey[] DROPDOWN_SPECIFIC_KEYS = {
        DROPDOWN_KEY_VALUE_LIST, DROPDOWN_CALLBACK, DROPDOWN_HINT
    };

    public static final PropertyKey[] DROPDOWN_ALL_KEYS =
            ObjectArrays.concat(
                    FieldProperties.FIELD_ALL_KEYS, DROPDOWN_SPECIFIC_KEYS, PropertyKey.class);

    private DropdownFieldProperties() {}

    public static @Nullable String getDropdownKeyByValue(
            PropertyModel dropdownField, @Nullable String value) {
        for (DropdownKeyValue keyValue : dropdownField.get(DROPDOWN_KEY_VALUE_LIST)) {
            if (keyValue.getValue().equals(value)) {
                return keyValue.getKey();
            }
        }
        return null;
    }

    public static @Nullable String getDropdownValueByKey(
            PropertyModel dropdownField, @Nullable String key) {
        for (DropdownKeyValue keyValue : dropdownField.get(DROPDOWN_KEY_VALUE_LIST)) {
            if (keyValue.getKey().equals(key)) {
                return keyValue.getValue();
            }
        }
        return null;
    }

    public static void setDropdownKey(PropertyModel dropdownField, @Nullable String key) {
        // The mValue can only be set to null if there is a hint.
        if (key == null && dropdownField.get(DROPDOWN_HINT) == null) {
            return;
        }
        dropdownField.set(FieldProperties.VALUE, key);
        Callback<String> fieldCallback = dropdownField.get(DROPDOWN_CALLBACK);
        if (fieldCallback != null) {
            fieldCallback.onResult(assumeNonNull(key));
        }
    }
}
