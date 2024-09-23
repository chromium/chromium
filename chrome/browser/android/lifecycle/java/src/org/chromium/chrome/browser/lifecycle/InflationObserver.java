// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.lifecycle;

/**
 * Implement this interface and register in {@link ActivityLifecycleDispatcher} to receive
 * inflation-related events.
 */
public interface InflationObserver extends LifecycleObserver {
    /**
     * Called immediately before the view hierarchy is inflated.
     * See {@link org.chromium.chrome.browser.init.BrowserParts#preInflationStartup()}.
     */
    void onPreInflationStartup();

    /**
     * Called immediately after the view hierarchy is inflated. It allows observers finishing high
     * priority tasks that the owner of the {@link ActivityLifecycleDispatcher} is waiting for, and
     * the owner can add additional tasks before observers' onPostInflationStartup() is called.
     * Note: you shouldn't override this function unless any subclass of AsyncInitializationActivity
     * waits for the observer's onInflationComplete() being completed. Overriding
     * onPostInflationStartup() is preferred. TODO(crbug.com/40134587): Removes this state.
     */
    default void onInflationComplete() {}

    /**
     * Called immediately after the view hierarchy is inflated.
     * See {@link org.chromium.chrome.browser.init.BrowserParts#postInflationStartup()}.
     */
    void onPostInflationStartup();
}
