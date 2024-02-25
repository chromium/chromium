// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** The different color picker types. */
@IntDef({ColorPickerType.TAB_GROUP})
@Retention(RetentionPolicy.SOURCE)
public @interface ColorPickerType {
    /** The tab group color picker component. */
    int TAB_GROUP = 1;
}
