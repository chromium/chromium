// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Tests for the {@link TimedCallbackDelayer} class.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TimedCallbackDelayerTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final EnsureAsyncPostingRule mPostingRule = new EnsureAsyncPostingRule();

    /**
     * Check that the callback is eventually called.
     */
    @Test
    public void testCallbackCalled() {
        // Arbitrary time delays in milliseconds.
        final long[] delays = {0, 2, 5432};
        for (long delay : delays) {
            Runnable callback = mock(Runnable.class);
            TimedCallbackDelayer delayer = new TimedCallbackDelayer(delay);

            delayer.delay(callback);
            verify(callback, never()).run();
            Robolectric.getForegroundThreadScheduler().advanceToLastPostedRunnable();
            verify(callback, times(1)).run();
        }
    }

    /**
     * Check that the callback is not called synchronously, even if the time delay is 0.
     */
    @Test
    public void testCallbackAsync() {
        Runnable callback = mock(Runnable.class);
        TimedCallbackDelayer delayer = new TimedCallbackDelayer(0);
        delayer.delay(callback);
        verify(callback, never()).run();
    }
}
