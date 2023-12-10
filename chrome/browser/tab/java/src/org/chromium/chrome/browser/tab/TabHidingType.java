// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@IntDef({
    TabHidingType.CHANGED_TABS,
    TabHidingType.ACTIVITY_HIDDEN,
    TabHidingType.REPARENTED,
    TabHidingType.TAB_SWITCHER_SHOWN
})
@Retention(RetentionPolicy.SOURCE)
public @interface TabHidingType {
    /** A tab was hidden due to other tab getting foreground. */
    int CHANGED_TABS = 0;

    /** A tab was hidden together with an activity. */
    int ACTIVITY_HIDDEN = 1;

    /** A tab was hidden while being reparented to a new activity. */
    int REPARENTED = 2;

    /** A tab was hidden while tab switcher was shown. */
    int TAB_SWITCHER_SHOWN = 3;
}
