// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors;

/** The interface for editor fields that handle validation, display errors, and can be updated. */
interface FieldView {
    /**
     * Validates the field.
     *
     * @return True if this field is valid.
     */
    boolean validate();

    /** @return True if this field is required. */
    boolean isRequired();

    /** Scrolls to and focuses the field to bring user's attention to it. */
    void scrollToAndFocus();

    /**
     * Shows '*' required field indicator according to the provided boolean flag
     *
     * @param showRequiredIndicator boolean flag indicating if this field should me marked with
     *         required indicator.
     */
    void setShowRequiredIndicator(boolean showRequiredIndicator);
}
