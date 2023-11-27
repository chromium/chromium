// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import org.junit.rules.ExternalResource;
import org.robolectric.Robolectric;
import org.robolectric.util.Scheduler;

/**
 * A Rule for ensuring that posted tasks are run asynchronously (and on demand). To do so, the rule
 * pauses the Roboelectric scheduler before running the test and restores its state afterwards.
 */
public class EnsureAsyncPostingRule extends ExternalResource {
    /** Remembers the paused state of the scheduler so that it can be restored after the test. */
    private boolean mWasSchedulerPaused;

    /** A reference to the scheduler which needs to be paused.*/
    private Scheduler mScheduler;

    @Override
    protected void before() {
        mScheduler = Robolectric.getForegroundThreadScheduler();
        mWasSchedulerPaused = mScheduler.isPaused();
        // Pause the scheduler, otherwise tasks which should run asynchronously will run
        // synchronously and confuse the tests.
        mScheduler.pause();
    }

    @Override
    protected void after() {
        if (!mWasSchedulerPaused) mScheduler.unPause();
    }
}
