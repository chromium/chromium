// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Types of tab card labels. */
@IntDef({TabCardLabelType.ACTIVITY_UPDATE, TabCardLabelType.PRICE_DROP})
@Retention(RetentionPolicy.SOURCE)
@interface TabCardLabelType {
    /** Price drops. */
    int PRICE_DROP = 0;

    /** New activity indicator. */
    int ACTIVITY_UPDATE = 1;
}
