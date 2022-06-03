// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

/**
 * A Runnable which postpones running a given callback until it is itself run for a pre-defined
 * number of times. It is inspired by the native //base/barrier_closure.*. Unlike the native code,
 * SingleThreadBarrierClosure is only meant to be used on a single thread and is not thread-safe.
 */
public final class SingleThreadBarrierClosure implements Runnable {
    /** Counts the remaining number of runs. */
    private int mRemainingRuns;

    /** The callback to be run when {@link #mRemainingRuns} reaches 0.*/
    private final Runnable mCallback;

    /**
     * Construct a {@link Runnable} such that the first {@code runsExpected-1} calls to {@link #run}
     * are a no-op and the last one runs {@code callback}.
     * @param runsExpected The number of total {@link #run} calls expected.
     * @param callback The callback to be run once called enough times.
     */
    public SingleThreadBarrierClosure(int runsExpected, Runnable callback) {
        assert runsExpected > 0;
        assert callback != null;
        mRemainingRuns = runsExpected;
        mCallback = callback;
    }

    @Override
    public void run() {
        if (mRemainingRuns == 0) return;
        --mRemainingRuns;
        if (mRemainingRuns == 0) mCallback.run();
    }

    /**
     * Whether the next call to {@link #run} runs {@link #mCallback}.
     * @return True if the next call to {@link #run} runs {@link #mCallback}, false otherwise.
     */
    public boolean isReady() {
        return mRemainingRuns == 1;
    }
}
