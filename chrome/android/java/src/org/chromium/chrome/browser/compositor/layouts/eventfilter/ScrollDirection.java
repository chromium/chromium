// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.eventfilter;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@IntDef({ScrollDirection.UNKNOWN, ScrollDirection.LEFT, ScrollDirection.RIGHT, ScrollDirection.DOWN,
        ScrollDirection.UP})
@Retention(RetentionPolicy.SOURCE)
public @interface ScrollDirection {
    int UNKNOWN = 0;
    int LEFT = 1;
    int RIGHT = 2;
    int DOWN = 3;
    int UP = 4;
}
