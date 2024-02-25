// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.invalidation;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.concurrent.TimeUnit;

/** Tests for the {@link ResumableDelayedTaskRunner}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ResumableDelayedTaskRunnerTest {
    @Mock Runnable mRunnable;

    @Before
    public void setup() {
        initMocks(this);
    }

    @Test
    public void testResume() {
        ResumableDelayedTaskRunner runner = new ResumableDelayedTaskRunner();
        runner.setRunnable(mRunnable, /* delayMs= */ 2000);
        runner.resume();

        Robolectric.getForegroundThreadScheduler().advanceBy(1500, TimeUnit.MILLISECONDS);
        verify(mRunnable, never()).run();

        Robolectric.getForegroundThreadScheduler().advanceBy(500, TimeUnit.MILLISECONDS);
        verify(mRunnable).run();
    }

    @Test
    public void testPause() {
        ResumableDelayedTaskRunner runner = new ResumableDelayedTaskRunner();
        runner.setRunnable(mRunnable, /* delayMs= */ 2000);
        runner.resume();

        runner.pause();

        Robolectric.getForegroundThreadScheduler().advanceBy(2000, TimeUnit.MILLISECONDS);
        verify(mRunnable, never()).run();

        runner.resume();

        Robolectric.getForegroundThreadScheduler().advanceBy(2000, TimeUnit.MILLISECONDS);
        verify(mRunnable).run();
    }

    @Test
    public void testCancel() {
        ResumableDelayedTaskRunner runner = new ResumableDelayedTaskRunner();
        runner.setRunnable(mRunnable, /* delayMs= */ 2000);
        runner.resume();

        runner.cancel();

        Robolectric.getForegroundThreadScheduler().advanceBy(2000, TimeUnit.MILLISECONDS);
        verify(mRunnable, never()).run();

        runner.resume();

        Robolectric.getForegroundThreadScheduler().advanceBy(2000, TimeUnit.MILLISECONDS);
        verify(mRunnable, never()).run();
    }
}
