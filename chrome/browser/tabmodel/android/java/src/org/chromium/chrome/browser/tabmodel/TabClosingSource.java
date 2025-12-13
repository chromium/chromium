// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Sources of tab closing event. */
@IntDef({
    TabClosingSource.UNKNOWN,
    TabClosingSource.TABLET_TAB_STRIP,
    TabClosingSource.KEYBOARD_SHORTCUT
})
@Retention(RetentionPolicy.SOURCE)
@NullMarked
public @interface TabClosingSource {
    /** All other sources. */
    int UNKNOWN = 0;

    /** Tab closing is from tablet tab strip. */
    int TABLET_TAB_STRIP = 1;

    /** Tab closing is from keyboard shortcut. */
    int KEYBOARD_SHORTCUT = 2;
}
