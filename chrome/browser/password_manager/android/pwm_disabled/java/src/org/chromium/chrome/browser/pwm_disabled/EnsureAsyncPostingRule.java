// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwm_disabled;

import static org.robolectric.Shadows.shadowOf;

import android.os.Looper;

import org.junit.rules.ExternalResource;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * A Rule for ensuring that posted tasks are run asynchronously (and on demand). To do so, the rule
 * pauses the main looper before running the test and restores its state afterwards.
 */
@NullMarked
class EnsureAsyncPostingRule extends ExternalResource {
    /** Remembers the paused state of the main looper so that it can be restored after the test. */
    private boolean mWasLooperPaused;

    /** The main looper, which needs to be paused. */
    private @Nullable ShadowLooper mMainLooper;

    @Override
    protected void before() {
        mMainLooper = shadowOf(Looper.getMainLooper());
        mWasLooperPaused = mMainLooper.isPaused();
        // Pause the main looper, otherwise tasks which should run asynchronously will run
        // synchronously and confuse the tests.
        mMainLooper.pause();
    }

    @Override
    protected void after() {
        if (!mWasLooperPaused) {
            assert mMainLooper != null;
            mMainLooper.unPause();
        }
    }
}
