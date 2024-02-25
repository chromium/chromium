// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.os.Handler;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.tab.Tab;

import java.util.LinkedList;
import java.util.List;
import java.util.concurrent.Callable;

/**
 * This class contains logic for capturing navigation info at an appropriate time.
 *
 * We want to capture navigation information after both onload and first meaningful paint have
 * triggered. We add a slight delay to avoid capturing during CPU intensive periods.
 *
 * If a capture has not been taken after a long amount of time or when the Tab is hidden, we also
 * capture.
 */
public class NavigationInfoCaptureTrigger {
    private static final int ONLOAD_DELAY_MS = 1000;
    private static final int ONLOAD_LONG_DELAY_MS = 15000;
    private static final int ONHIDE_DELAY_MS = 1000;
    private static final int FMP_DELAY_MS = 3000;

    private final Callback<Tab> mCapture;
    private final Handler mUiThreadHandler = new Handler(ThreadUtils.getUiThreadLooper());
    private final List<Runnable> mPendingRunnables = new LinkedList<>();

    private boolean mOnloadTriggered;
    private boolean mFirstMeaningfulPaintTriggered;
    private boolean mCaptureTaken;

    public NavigationInfoCaptureTrigger(Callback<Tab> capture) {
        mCapture = capture;
    }

    /** Notifies that a page navigation has occurred and state should reset. */
    public void onNewNavigation() {
        mOnloadTriggered = false;
        mFirstMeaningfulPaintTriggered = false;
        mCaptureTaken = false;
        clearPendingRunnables();
    }

    /** Notifies that onload has occurred. */
    public void onLoadFinished(Tab tab) {
        mOnloadTriggered = true;
        captureDelayedIf(tab, () -> mFirstMeaningfulPaintTriggered, ONLOAD_DELAY_MS);
        captureDelayed(tab, ONLOAD_LONG_DELAY_MS);
    }

    /** Notifies that first meaningful paint has occurred. */
    public void onFirstMeaningfulPaint(Tab tab) {
        mFirstMeaningfulPaintTriggered = true;
        captureDelayedIf(tab, () -> mOnloadTriggered, FMP_DELAY_MS);
    }

    /** Notifies that the Tab has been hidden. */
    public void onHide(Tab tab) {
        captureDelayed(tab, ONHIDE_DELAY_MS);
    }

    private void clearPendingRunnables() {
        for (Runnable pendingRunnable : mPendingRunnables) {
            mUiThreadHandler.removeCallbacks(pendingRunnable);
        }
        mPendingRunnables.clear();
    }

    /** Posts a CaptureRunnable that will capture navigation info after the delay (ms). */
    private void captureDelayed(Tab tab, long delay) {
        captureDelayedIf(tab, () -> true, delay);
    }

    /**
     * Posts a CaptureRunnable that will capture navigation info after the delay (ms) if the check
     * passes.
     */
    private void captureDelayedIf(Tab tab, Callable<Boolean> check, long delay) {
        if (mCaptureTaken) return;
        Runnable runnable = new CaptureRunnable(tab, check);
        mPendingRunnables.add(runnable);
        mUiThreadHandler.postDelayed(runnable, delay);
    }

    /**
     * A Runnable that when executes ensures that no capture has already been taken and that the
     * check passes, then captures the navigation info and clears all other pending Runnables.
     */
    private class CaptureRunnable implements Runnable {
        private final Callable<Boolean> mCheck;
        private final Tab mTab;

        public CaptureRunnable(Tab tab, Callable<Boolean> check) {
            mCheck = check;
            mTab = tab;
        }

        @Override
        public void run() {
            assert !mCaptureTaken;

            try {
                if (!mCheck.call()) return;
            } catch (Exception e) {
                // In case mCheck.call throws an exception (which it shouldn't, but it's part of
                // Callable#call's signature).
                throw new RuntimeException(e);
            }

            mCapture.onResult(mTab);
            mCaptureTaken = true;

            clearPendingRunnables();
        }
    }
}
