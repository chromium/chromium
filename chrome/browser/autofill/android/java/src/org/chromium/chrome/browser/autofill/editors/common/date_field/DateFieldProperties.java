// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors.common.date_field;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties;
import org.chromium.ui.modelutil.PropertyKey;

/** Properties specific for the dropdown fields. */
@NullMarked
public class DateFieldProperties {

    public static final PropertyKey[] DATE_ALL_KEYS = FieldProperties.FIELD_ALL_KEYS;

    private DateFieldProperties() {}
}
