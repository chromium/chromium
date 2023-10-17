// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors;

/** A test-only observer for Editor UI. */
public interface EditorObserverForTest {
    /** Called when edit dialog is showing. */
    void onEditorReadyToEdit();

    /**
     * Called when editor validation completes with error. This can happen, for example, when user
     * doesn't fill in required fields.
     */
    void onEditorValidationError();

    /** Called when an editor field text has changed. */
    void onEditorTextUpdate();

    /** Called when the editor is dismissed. */
    void onEditorDismiss();

    /** Called when the delete confirmation dialog shown. */
    void onEditorConfirmationDialogShown();
}
