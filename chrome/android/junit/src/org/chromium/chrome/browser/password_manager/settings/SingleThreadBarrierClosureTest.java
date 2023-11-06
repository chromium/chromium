// Copyright 2018 The Chromium Authors
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
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for the {@link SingleThreadBarrierClosure} class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SingleThreadBarrierClosureTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule public final EnsureAsyncPostingRule mPostingRule = new EnsureAsyncPostingRule();

    /** Check that the callback is posted after as many signals as specified. */
    @Test
    public void testCallbackPosted() {
        // Arbitrary counts of signals to try.
        final int[] signalCounts = {1, 2, 7};
        for (int signalCount : signalCounts) {
            Runnable callback = mock(Runnable.class);
            SingleThreadBarrierClosure barrierClosure =
                    new SingleThreadBarrierClosure(signalCount, callback);

            for (int i = 0; i < signalCount - 1; ++i) barrierClosure.run();
            // No callback yet, too few run() invocations.
            verify(callback, never()).run();

            barrierClosure.run();
            verify(callback, times(1)).run();

            barrierClosure.run();
            // Ensure that further run() calls are quietly ignored: the callback was not run a
            // second time (the recorded call count is still at 1).
            verify(callback, times(1)).run();
        }
    }
}
