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
    InAppUpdateSnackbarEvent.DISMISSED_BY_LIFECYCLE
})
@Retention(RetentionPolicy.SOURCE)
public @interface InAppUpdateSnackbarEvent {
    int IMPRESSION = 0;
    int ACTION_TAPPED = 1;
    int DISMISSED_NO_ACTION = 2;
    int DISMISSED_BY_LIFECYCLE = 3;
    int COUNT = 4;
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:InAppUpdateSnackbarEvent)
