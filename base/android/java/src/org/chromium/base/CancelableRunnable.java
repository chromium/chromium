// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.chromium.build.BuildConfig;

/**
 * CancelableRunnable is a Runnable class that can be canceled. A canceled task is not removed from
 * the task queue - it instead becomes a noop.
 *
 * <p>This class is *not* threadsafe, and is only for canceling tasks from the thread on which it
 * would be run.
 */
public class CancelableRunnable implements Runnable {
    private Runnable mRunnable;
    private Thread mExpectedThread;

    public CancelableRunnable(Runnable runnable) {
        mRunnable = runnable;
    }

    public void cancel() {
        mRunnable = null;
        if (BuildConfig.ENABLE_ASSERTS) {
            mExpectedThread = Thread.currentThread();
        }
    }

    @Override
    public void run() {
        assert mExpectedThread == null || Thread.currentThread() == mExpectedThread
                : "Canceling thread: " + mExpectedThread.getName();
        if (mRunnable != null) mRunnable.run();
    }
}
