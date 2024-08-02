// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Types of tab closure events. */
@IntDef({TabCloseType.SINGLE, TabCloseType.MULTIPLE, TabCloseType.ALL})
@Retention(RetentionPolicy.SOURCE)
@interface TabCloseType {
    /** A single tab is closing. */
    int SINGLE = 0;

    /** Multiple tabs are closing. This may be a tab group or just a selection of tabs. */
    int MULTIPLE = 1;

    /** All tabs are closing. */
    int ALL = 2;
}
