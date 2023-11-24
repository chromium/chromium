// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Visibility and animation states for a player. */
@IntDef({
    VisibilityState.GONE,
    VisibilityState.SHOWING,
    VisibilityState.VISIBLE,
    VisibilityState.HIDING
})
@Retention(RetentionPolicy.SOURCE)
public @interface VisibilityState {
    /** Player isn't on the screen or transitioning. Default state. */
    int GONE = 0;

    /** Player is transitioning from GONE to VISIBLE. */
    int SHOWING = 1;

    /** Player is open and not transitioning. */
    int VISIBLE = 2;

    /** Player is transitioning from VISIBLE to GONE. */
    int HIDING = 3;
}
