// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha.inline;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Events for the in-app update discovery and restart snackbars.
 *
 * <p>These values are persisted to logs. Entries should not be renumbered and numeric values should
 * never be reused.
 */
// LINT.IfChange(InAppUpdateSnackbarEvent)
@NullMarked
@IntDef({
    InAppUpdateSnackbarEvent.IMPRESSION,
    InAppUpdateSnackbarEvent.ACTION_TAPPED,
    InAppUpdateSnackbarEvent.DISMISSED_NO_ACTION,
    InAppUpdateSnackbarEvent.DISMISSED_BY_LIFECYCLE,
    InAppUpdateSnackbarEvent.SKIPPED_STARTUP_PROMO,
    InAppUpdateSnackbarEvent.SKIPPED_MODAL_DIALOG,
    InAppUpdateSnackbarEvent.SKIPPED_BOTTOM_SHEET
})
@Retention(RetentionPolicy.SOURCE)
public @interface InAppUpdateSnackbarEvent {
    int IMPRESSION = 0;
    int ACTION_TAPPED = 1;
    int DISMISSED_NO_ACTION = 2;
    int DISMISSED_BY_LIFECYCLE = 3;
    // The SKIPPED_* values record that the snackbar was not shown because other UI was competing
    // for the user's attention. Only recorded for the discovery snackbar.
    int SKIPPED_STARTUP_PROMO = 4;
    int SKIPPED_MODAL_DIALOG = 5;
    int SKIPPED_BOTTOM_SHEET = 6;
    int COUNT = 7;
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:InAppUpdateSnackbarEvent)
