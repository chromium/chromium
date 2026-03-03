// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors.common.date_field;

import com.google.common.collect.ObjectArrays;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;

/** Properties specific for the dropdown fields. */
@NullMarked
public class DateFieldProperties {
    public static final WritableBooleanPropertyKey DATE_VALID =
            new WritableBooleanPropertyKey("date_valid");

    public static final PropertyKey[] DATE_SPECIFIC_KEYS = {DATE_VALID};

    public static final PropertyKey[] DATE_ALL_KEYS =
            ObjectArrays.concat(
                    FieldProperties.FIELD_ALL_KEYS, DATE_SPECIFIC_KEYS, PropertyKey.class);

    private DateFieldProperties() {}
}
