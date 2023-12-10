// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.os.PowerManager;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.omaha.OmahaBase;

/**
 * Class to delay interactions with other classes based on whether the app is in the foreground
 * or not and whether the device is considered interactive. Useful for running actions that should
 * not occur while the phone's screen is off, i.e. when the user expects the device to be sleeping.
 *
 * This class does not do any system monitoring on its own, and requires the owner to invoke
 * {@link #onForegroundSessionStart()} and {@link #onForegroundSessionEnd()} when the foreground
 * session starts and ends respectively.
 *
 * The class also verifies that the screen is on at the time of trying to schedule the delayed task
 * as well as right before trying to execute the task, in the case where the screen is turned off,
 * but the app is still considered to be in the foreground.
 *
 * If the app is first in the foreground, leading to a scheduled task, then goes to the background,
 * before coming to the foreground again in quick succession, the timer should be reset.
 *
 * When conditions are right, executes code in {@link OmahaServiceStartDelayer.OmahaRunnable#run()}.
 */
public class OmahaServiceStartDelayer {
    /**
     * @return true iff the device is currently considered to be in an interactive state.
     */
    private static boolean isInteractive() {
        PowerManager powerManager =
                (PowerManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.POWER_SERVICE);
        return powerManager.isInteractive();
    }

    /**
     * ANRs are triggered if the app fails to respond to a touch event within 5 seconds. Posting
     * this with a delay of 5 seconds lets Chrome prioritize more urgent tasks.
     */
    private static final long MS_DELAY_TO_RUN = 5000;

    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private Runnable mOmahaRunnable = OmahaBase::onForegroundSessionStart;
    private Runnable mRunnableTask;

    /** See {@link ChromeActivitySessionTracker#onForegroundSessionStart()}. */
    public void onForegroundSessionStart() {
        ThreadUtils.assertOnUiThread();
        assert Looper.getMainLooper() == Looper.myLooper();

        if (!isInteractive()) return;
        if (hasRunnableController()) return;

        mRunnableTask =
                () -> {
                    if (isInteractive()) mOmahaRunnable.run();
                    cancelAndCleanup();
                };
        mHandler.postDelayed(mRunnableTask, MS_DELAY_TO_RUN);
    }

    /** See {@link ChromeActivitySessionTracker#onForegroundSessionEnd()}. */
    public void onForegroundSessionEnd() {
        ThreadUtils.assertOnUiThread();
        assert Looper.getMainLooper() == Looper.myLooper();

        cancelAndCleanup();
    }

    @VisibleForTesting
    void cancelAndCleanup() {
        if (hasRunnableController()) {
            mHandler.removeCallbacks(mRunnableTask);
            mRunnableTask = null;
        }
    }

    /** Sets the runnable that contains the actions to do when the device is interactive. */
    void setOmahaRunnableForTesting(Runnable runnable) {
        cancelAndCleanup();
        mOmahaRunnable = runnable;
    }

    /**
     * @return True if there is currently a Runnable that is waiting for execution.
     */
    @VisibleForTesting
    boolean hasRunnableController() {
        return mRunnableTask != null;
    }
}
