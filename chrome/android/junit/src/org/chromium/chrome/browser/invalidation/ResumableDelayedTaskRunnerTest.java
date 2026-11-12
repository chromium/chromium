// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.invalidation;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.concurrent.TimeUnit;

/** Tests for the {@link ResumableDelayedTaskRunner}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ResumableDelayedTaskRunnerTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock Runnable mRunnable;

    @Test
    public void testResume() {
        ResumableDelayedTaskRunner runner = new ResumableDelayedTaskRunner();
        runner.setRunnable(mRunnable, /* delayMs= */ 2000);
        runner.resume();

        ShadowLooper.idleMainLooper(1500, TimeUnit.MILLISECONDS);
        verify(mRunnable, never()).run();

        ShadowLooper.idleMainLooper(500, TimeUnit.MILLISECONDS);
        verify(mRunnable).run();
    }

    @Test
    public void testPause() {
        ResumableDelayedTaskRunner runner = new ResumableDelayedTaskRunner();
        runner.setRunnable(mRunnable, /* delayMs= */ 2000);
        runner.resume();

        runner.pause();

        ShadowLooper.idleMainLooper(2000, TimeUnit.MILLISECONDS);
        verify(mRunnable, never()).run();

        runner.resume();

        ShadowLooper.idleMainLooper(2000, TimeUnit.MILLISECONDS);
        verify(mRunnable).run();
    }

    @Test
    public void testCancel() {
        ResumableDelayedTaskRunner runner = new ResumableDelayedTaskRunner();
        runner.setRunnable(mRunnable, /* delayMs= */ 2000);
        runner.resume();

        runner.cancel();

        ShadowLooper.idleMainLooper(2000, TimeUnit.MILLISECONDS);
        verify(mRunnable, never()).run();

        runner.resume();

        ShadowLooper.idleMainLooper(2000, TimeUnit.MILLISECONDS);
        verify(mRunnable, never()).run();
    }
}
