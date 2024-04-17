// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** An int def enum to represent one of the four corners of a rectangle/square. */
@IntDef({Corner.TOP_LEFT, Corner.TOP_RIGHT, Corner.BOTTOM_LEFT, Corner.BOTTOM_RIGHT})
@Retention(RetentionPolicy.SOURCE)
public @interface Corner {
    // The ordering here must match the order Android uses, such
    // GradientDrawable#setCornerRadii(), which is clockwise starting with the top left.
    int TOP_LEFT = 0;
    int TOP_RIGHT = 1;
    int BOTTOM_RIGHT = 2;
    int BOTTOM_LEFT = 3;
}
