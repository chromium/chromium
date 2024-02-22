// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.lifecycle;

/**
 * Implement this interface and register in {@link ActivityLifecycleDispatcher} to receive
 * onUserLeaveHint events.
 */
public interface OnUserLeaveHintObserver extends LifecycleObserver {
    /** Called when an activity is about to go into the background as the result of user choice. */
    void onUserLeaveHint();
}
