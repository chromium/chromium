// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.base;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Histogram enum to monitor DexFixer. */
@IntDef({
    DexFixerReason.STAT_FAILED,
    DexFixerReason.FAILED_TO_RUN,
    DexFixerReason.NOT_NEEDED,
    DexFixerReason.O_MR1_AFTER_UPDATE,
    DexFixerReason.O_MR1_CORRUPTED,
    DexFixerReason.O_MR1_IO_EXCEPTION,
    DexFixerReason.NOT_READABLE
})
@Retention(RetentionPolicy.SOURCE)
public @interface DexFixerReason {
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    int STAT_FAILED = 0;
    int FAILED_TO_RUN = 1;
    int NOT_NEEDED = 2;
    // Values greater than NOT_NEEDED trigger Dexopt.
    int O_MR1_AFTER_UPDATE = 5;
    int O_MR1_CORRUPTED = 6;
    int O_MR1_IO_EXCEPTION = 7;
    int NOT_READABLE = 8;
    int COUNT = 9;
}
