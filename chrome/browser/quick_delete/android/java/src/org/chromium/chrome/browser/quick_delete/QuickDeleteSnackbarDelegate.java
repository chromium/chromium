// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;

/**
 * A delegate responsible for providing logic around the quick delete {@link Snackbar}.
 *
 * TODO(crbug.com/1412087): Add the implementation to actually show the snack-bar.
 */
class QuickDeleteSnackbarDelegate {
    private final @NonNull SnackbarManager mSnackbarManager;

    /**
     * @param snackbarManager A {@link SnackbarManager} responsible for showing the quick delete
     *         {@link Snackbar}.
     */
    QuickDeleteSnackbarDelegate(@NonNull SnackbarManager snackbarManager) {
        mSnackbarManager = snackbarManager;
    }

    /**
     * A method to show the quick delete snack-bar.
     *
     * TODO(crbug.com/1412087): Prepare and show the snack-bar.
     */
    void showSnackbar() {}
}
