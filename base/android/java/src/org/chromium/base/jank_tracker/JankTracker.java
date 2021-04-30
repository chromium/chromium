// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.jank_tracker;

import android.app.Activity;
import android.os.Build.VERSION_CODES;

import androidx.annotation.RequiresApi;

/**
 * Class for recording janky frame metrics for a specific Activity.
 *
 * It should be constructed when the activity is created, recording starts and stops automatically
 * based on activity state. When the activity is being destroyed {@link #destroy()} should be called
 * to clear the activity state observer. All methods should be called from the UI thread.
 */
@RequiresApi(api = VERSION_CODES.N)
public final class JankTracker {
    private final JankActivityTracker mActivityTracker;

    /**
     * Creates a new JankTracker instance tracking UI rendering of an activity. Metric recording
     * starts when the activity starts, and it's paused when the activity stops.
     */
    public JankTracker(Activity activity) {
        mActivityTracker = JankActivityTracker.create(activity);
        mActivityTracker.initialize();
    }

    /**
     * Stops listening for Activity state changes.
     */
    public void destroy() {
        mActivityTracker.destroy();
    }
}
