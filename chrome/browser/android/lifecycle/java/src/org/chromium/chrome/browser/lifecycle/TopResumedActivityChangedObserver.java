// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.lifecycle;

import android.app.Activity;

import org.chromium.build.annotations.NullMarked;

/**
 * Implement this interface and register in {@link ActivityLifecycleDispatcher} to receive
 * onTopResumedActivityChanged events.
 */
@NullMarked
public interface TopResumedActivityChangedObserver extends LifecycleObserver {
    /**
     * Called when an activity gets or loses the top resumed position in the system. See {@link
     * Activity#onTopResumedActivityChanged(boolean)}.
     */
    void onTopResumedActivityChanged(boolean isTopResumedActivity);
}
