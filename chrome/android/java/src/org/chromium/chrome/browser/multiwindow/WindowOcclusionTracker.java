// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.ActivityWindowAndroid;

/** Monitors the position, size, and z-order of Chrome windows for occlusion tracking. */
@NullMarked
public class WindowOcclusionTracker {
    // |TAG| can be at most 20 characters, so the class name itself is too long.
    private static final String TAG = "OcclusionTracking";
    private static final boolean DEBUG_LOGGING = false;
    private static @Nullable WindowOcclusionTracker sInstance;

    private WindowOcclusionTracker() {}

    public static WindowOcclusionTracker getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) {
            sInstance = new WindowOcclusionTracker();
        }
        return sInstance;
    }

    public static void setInstanceForTesting(WindowOcclusionTracker instance) {
        sInstance = instance;
    }

    /**
     * Starts tracking the given window.
     *
     * @param windowAndroid The window to track.
     */
    public void track(ActivityWindowAndroid windowAndroid) {
        ThreadUtils.assertOnUiThread();
        if (DEBUG_LOGGING) Log.i(TAG, "Tracking window: %s", windowAndroid);
        // TODO(488905916) - Implement tracking.
    }

    /**
     * Stops tracking the given window.
     *
     * @param windowAndroid The window to stop tracking.
     */
    public void untrack(ActivityWindowAndroid windowAndroid) {
        ThreadUtils.assertOnUiThread();
        if (DEBUG_LOGGING) Log.i(TAG, "Untracking window: %s", windowAndroid);
        // TODO(488905916) - Implement tracking.
    }
}
