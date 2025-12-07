// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.base;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Enum to monitor DexFixer. */
@IntDef({DexFixerReason.STAT_FAILED, DexFixerReason.NOT_NEEDED, DexFixerReason.NOT_READABLE})
@Retention(RetentionPolicy.SOURCE)
@NullMarked
public @interface DexFixerReason {
    int STAT_FAILED = 0;
    int NOT_NEEDED = 1;
    // Values greater than NOT_NEEDED trigger Dexopt.
    int NOT_READABLE = 2;
}
