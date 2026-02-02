// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors.common.field;

import org.chromium.build.annotations.NullMarked;

/** The interface for editor fields that handle validation, display errors, and can be updated. */
@NullMarked
public interface FieldView {
    /** The indicator for input fields that are required. */
    String REQUIRED_FIELD_INDICATOR = "*";

    /**
     * Validates the field.
     *
     * @return True if this field is valid.
     */
    boolean validate();

    /**
     * @return True if this field is required.
     */
    boolean isRequired();

    /** Scrolls to and focuses the field to bring user's attention to it. */
    void scrollToAndFocus();
}
