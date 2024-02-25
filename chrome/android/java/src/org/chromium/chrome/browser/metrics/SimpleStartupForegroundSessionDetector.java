// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

/**
 * Determines whether the browsing session early at startup is good enough for startup metrics.
 * Transitioning the activity to background would recommend omitting the metrics because of
 * background restrictions and throttling. Must be subscribed to pause/resume events.
 */
public class SimpleStartupForegroundSessionDetector {
    private static boolean sSessionDiscarded;
    private static boolean sReachedForeground;

    public static void onTransitionToForeground() {
        if (sReachedForeground) {
            sSessionDiscarded = true;
            return;
        }
        sReachedForeground = true;
    }

    public static void discardSession() {
        sSessionDiscarded = true;
    }

    public static boolean isSessionDiscarded() {
        return sSessionDiscarded;
    }

    /** Returns whether the startup happened cleanly in the foreground. */
    public static boolean runningCleanForegroundSession() {
        return sReachedForeground && !sSessionDiscarded;
    }

    public static void resetForTesting() {
        sReachedForeground = false;
        sSessionDiscarded = false;
    }
}
