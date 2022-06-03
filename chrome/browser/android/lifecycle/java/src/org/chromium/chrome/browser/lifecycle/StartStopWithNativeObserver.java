// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.lifecycle;

/**
 * Implement this interface and register in {@link ActivityLifecycleDispatcher} to receive start and
 * stop with native events.
 */
public interface StartStopWithNativeObserver extends LifecycleObserver {
    /**
     * Called when activity is started, provided that native is initialized.
     * If native is not initialized at that point, the call is postponed until it is.
     */
    void onStartWithNative();

    /**
     * Similar to {@link #onStartWithNative}, but for the stop event.
     */
    void onStopWithNative();
}
