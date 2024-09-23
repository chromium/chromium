// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.lifecycle;

import android.app.Activity;

/**
 * Implement this interface and register in {@link ActivityLifecycleDispatcher} to receive
 * onTopResumedActivityChanged events.
 */
public interface TopResumedActivityChangedObserver extends LifecycleObserver {
    /**
     * Called when an activity gets or loses the top resumed position in the system. See {@link
     * Activity#onTopResumedActivityChanged(boolean)}.
     */
    void onTopResumedActivityChanged(boolean isTopResumedActivity);
}
