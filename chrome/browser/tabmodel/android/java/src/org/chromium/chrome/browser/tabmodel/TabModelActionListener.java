// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.components.browser_ui.widget.ActionConfirmationResult;

/**
 * Listener to get updates for actions that may show speedbump dialogs when performing operations on
 * a {@link TabModel}. See {@link TabRemover} and {@link TabUngrouper}.
 */
public interface TabModelActionListener {
    /**
     * Called with the result of showing the action confirmation dialog for the action. This is
     * guaranteed to be called, and may be called synchronously if no dialog is shown and the action
     * will proceed synchronously. This will be called after the action is triggered.
     *
     * @param result The {@link ActionConfirmationResult}.
     */
    default void onConfirmationDialogResult(@ActionConfirmationResult int result) {}
}
