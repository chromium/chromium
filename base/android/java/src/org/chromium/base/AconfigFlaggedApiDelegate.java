// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.app.ActivityManager;

import org.chromium.build.annotations.NullMarked;

/** Interface to call unreleased Android APIs that are guarded by aconfig flags. */
@NullMarked
public interface AconfigFlaggedApiDelegate {
    /**
     * Calls the {@link android.app.ActivityManager#isTaskMoveAllowedOnDisplay} method if supported,
     * otherwise returns false.
     *
     * @param am {@link android.app.ActivityManager} on which the method should be called.
     */
    default boolean isTaskMoveAllowedOnDisplay(ActivityManager am, int displayId) {
        return false;
    }
}
