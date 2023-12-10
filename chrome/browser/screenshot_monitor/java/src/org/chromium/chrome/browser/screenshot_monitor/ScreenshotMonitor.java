// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.screenshot_monitor;

import org.chromium.base.ThreadUtils;

/** Base class for detecting screenshots and notifying the {@code ScreenshotMonitorDelegate}. */
public abstract class ScreenshotMonitor {
    private final ScreenshotMonitorDelegate mDelegate;
    private boolean mIsMonitoring;

    public ScreenshotMonitor(ScreenshotMonitorDelegate delegate) {
        mDelegate = delegate;
    }

    /** Start observing screenshot actions. */
    public final void startMonitoring() {
        ThreadUtils.assertOnUiThread();
        if (mIsMonitoring) return;
        mIsMonitoring = true;
        setUpMonitoring(true);
    }

    /** Stop observing screenshot actions. */
    public final void stopMonitoring() {
        ThreadUtils.assertOnUiThread();
        if (!mIsMonitoring) return;
        mIsMonitoring = false;
        setUpMonitoring(false);
    }

    /**
     * Implementation class method to do the actual monitoring for screenshots.
     * @param monitor Whether or not to monitor for screenshots.
     */
    protected abstract void setUpMonitoring(boolean monitor);

    /**
     * Helper method meant to be used by subclasses to notify the {@code ScreenshotMonitorDelegate}
     * of screenshot actions.
     */
    protected final void notifyDelegate() {
        ThreadUtils.assertOnUiThread();
        if (!mIsMonitoring) return;
        mDelegate.onScreenshotTaken();
    }
}
