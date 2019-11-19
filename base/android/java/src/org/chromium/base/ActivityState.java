// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A set of states that represent the last state change of an Activity.
 */
@Retention(RetentionPolicy.SOURCE)
@IntDef({ActivityState.CREATED, ActivityState.STARTED, ActivityState.RESUMED, ActivityState.PAUSED,
        ActivityState.STOPPED, ActivityState.DESTROYED})
public @interface ActivityState {
    /**
     * Represents Activity#onCreate().
     */
    int CREATED = 1;

    /**
     * Represents Activity#onStart().
     */
    int STARTED = 2;

    /**
     * Represents Activity#onResume().
     */
    int RESUMED = 3;

    /**
     * Represents Activity#onPause().
     */
    int PAUSED = 4;

    /**
     * Represents Activity#onStop().
     */
    int STOPPED = 5;

    /**
     * Represents Activity#onDestroy().  This is also used when the state of an Activity is unknown.
     */
    int DESTROYED = 6;
}
