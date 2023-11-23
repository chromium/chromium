// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.base.task.test;

import android.os.Looper;

import org.junit.rules.ExternalResource;
import org.robolectric.Shadows;
import org.robolectric.android.util.concurrent.PausedExecutorService;

import org.chromium.base.task.PostTask;

import java.util.concurrent.TimeUnit;

/** Allows tests to manually schedule background tasks posted via PostTask APIs. */
public class PausedExecutorTestRule extends ExternalResource {
    private final PausedExecutorService mPausedExecutor = new PausedExecutorService();

    @Override
    protected void before() {
        PostTask.setPrenativeThreadPoolExecutorForTesting(mPausedExecutor);
    }

    @Override
    protected void after() {
        mPausedExecutor.shutdownNow();
        try {
            mPausedExecutor.awaitTermination(1, TimeUnit.SECONDS);
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * Runs all currently background tasks and waits for them to finish.
     * Warning: This will deadlock if a background tasks blocks on the UI thread.
     * @return Whether any background tasks were run.
     */
    public boolean runAllBackgroundAndUi() {
        int taskCount = 0;
        for (int i = 0; i < 100; ++i) {
            Shadows.shadowOf(Looper.getMainLooper()).idle();
            if (!mPausedExecutor.hasQueuedTasks()) {
                return taskCount > 0;
            }
            taskCount += mPausedExecutor.runAll();
        }
        throw new AssertionError("Infinite loop of background->foreground->background jobs");
    }
}
