// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Test class for {@link CallbackController}, which also describes typical usage. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CallbackControllerTest {
    /** Callbacks in this test act on {@code CallbackTarget}. */
    private static class CallbackTarget {
        public void runnableTarget() {}

        public void callbackTarget(boolean arg) {}
    }

    @Test
    public void testInstanceCallback() {
        CallbackController callbackController = new CallbackController();
        CallbackTarget target = Mockito.mock(CallbackTarget.class);
        Callback<Boolean> wrapped = callbackController.makeCancelable(target::callbackTarget);

        wrapped.onResult(true);
        verify(target).callbackTarget(true);

        // Execution possible multiple times.
        wrapped.onResult(true);
        verify(target, times(2)).callbackTarget(true);

        // Won't trigger after CallbackController is destroyed.
        callbackController.destroy();
        wrapped.onResult(true);
        verifyNoMoreInteractions(target);
    }

    @Test
    public void testInstanceRunnable() {
        CallbackController callbackController = new CallbackController();
        CallbackTarget target = Mockito.mock(CallbackTarget.class);
        Runnable wrapped = callbackController.makeCancelable(target::runnableTarget);

        wrapped.run();
        verify(target).runnableTarget();

        // Execution possible multiple times.
        wrapped.run();
        verify(target, times(2)).runnableTarget();

        // Won't trigger after CallbackController is destroyed.
        callbackController.destroy();
        wrapped.run();
        verifyNoMoreInteractions(target);
    }

    @Test
    public void testLambdaCallback() {
        CallbackController callbackController = new CallbackController();
        CallbackTarget target = Mockito.mock(CallbackTarget.class);
        Callback<Boolean> wrapped =
                callbackController.makeCancelable(value -> target.callbackTarget(value));

        wrapped.onResult(true);
        verify(target).callbackTarget(true);

        // Execution possible multiple times.
        wrapped.onResult(true);
        verify(target, times(2)).callbackTarget(true);

        // Won't trigger after CallbackController is destroyed.
        callbackController.destroy();
        wrapped.onResult(true);
        verifyNoMoreInteractions(target);
    }

    @Test
    public void testLambdaRunnable() {
        CallbackController callbackController = new CallbackController();
        CallbackTarget target = Mockito.mock(CallbackTarget.class);
        Runnable wrapped = callbackController.makeCancelable(() -> target.runnableTarget());

        wrapped.run();
        verify(target).runnableTarget();

        // Execution possible multiple times.
        wrapped.run();
        verify(target, times(2)).runnableTarget();

        // Won't trigger after CallbackController is destroyed.
        callbackController.destroy();
        wrapped.run();
        verifyNoMoreInteractions(target);
    }

    @Test
    public void testNestedRunnable() {
        CallbackController callbackController = new CallbackController();
        CallbackTarget target = Mockito.mock(CallbackTarget.class);
        Runnable makeCancelableAndRun =
                () -> callbackController.makeCancelable(target::runnableTarget).run();
        Runnable wrapped = callbackController.makeCancelable(makeCancelableAndRun);

        // Inside of the wrapping CancelableRunnable#run(), the inner make/run is performed. This
        // verifies the lock is reentrant.
        wrapped.run();
        verify(target).runnableTarget();

        wrapped.run();
        verify(target, times(2)).runnableTarget();

        callbackController.destroy();
        wrapped.run();
        verifyNoMoreInteractions(target);
    }

    @Test
    public void testNestedDestroy() {
        // Destroying from within a callback should work fine.
        CallbackController callbackController = new CallbackController();
        CallbackTarget target = Mockito.mock(CallbackTarget.class);
        Runnable wrapped = callbackController.makeCancelable(callbackController::destroy);

        wrapped.run();
    }
}
