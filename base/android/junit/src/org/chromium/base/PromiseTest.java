// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Promise.UnhandledRejectionException;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.function.Function;

/** Unit tests for {@link Promise}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.LEGACY)
public class PromiseTest {
    // We need a simple mutable reference type for testing.
    private static class Value {
        private int mValue;

        public int get() {
            return mValue;
        }

        public void set(int value) {
            mValue = value;
        }
    }

    /** Tests that the callback is called on fulfillment. */
    @Test
    public void callback() {
        final Value value = new Value();

        Promise<Integer> promise = new Promise<>();
        promise.then(PromiseTest.setValue(value, 1));

        assertEquals(value.get(), 0);

        promise.fulfill(1);
        assertEquals(value.get(), 1);
    }

    /** Tests that multiple callbacks are called. */
    @Test
    public void multipleCallbacks() {
        final Value value = new Value();

        Promise<Integer> promise = new Promise<>();
        Callback<Integer> callback =
                unusedArg -> {
                    value.set(value.get() + 1);
                };
        promise.then(callback);
        promise.then(callback);

        assertEquals(value.get(), 0);

        promise.fulfill(0);
        assertEquals(value.get(), 2);
    }

    /** Tests that a callback is called immediately when given to a fulfilled Promise. */
    @Test
    public void callbackOnFulfilled() {
        final Value value = new Value();

        Promise<Integer> promise = Promise.fulfilled(0);
        assertEquals(value.get(), 0);

        promise.then(PromiseTest.setValue(value, 1));

        assertEquals(value.get(), 1);
    }

    /** Tests that promises can chain synchronous functions correctly. */
    @Test
    public void promiseChaining() {
        Promise<Integer> promise = new Promise<>();
        final Value value = new Value();

        promise.then((Integer arg) -> arg.toString())
                .then((String arg) -> arg + arg)
                .then(
                        result -> {
                            value.set(result.length());
                        });

        promise.fulfill(123);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertEquals(6, value.get());
    }

    /** Tests that promises can chain asynchronous functions correctly. */
    @Test
    public void promiseChainingAsyncFunctions() {
        Promise<Integer> promise = new Promise<>();
        final Value value = new Value();

        final Promise<String> innerPromise = new Promise<>();

        promise.then(arg -> innerPromise)
                .then(
                        result -> {
                            value.set(result.length());
                        });

        assertEquals(0, value.get());

        promise.fulfill(5);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertEquals(0, value.get());

        innerPromise.fulfill("abc");
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertEquals(3, value.get());
    }

    /** Tests that a Promise that does not use its result does not throw on rejection. */
    @Test
    public void rejectPromiseNoCallbacks() {
        Promise<Integer> promise = new Promise<>();

        boolean caught = false;
        try {
            promise.reject();
            ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        } catch (UnhandledRejectionException e) {
            caught = true;
        }
        assertFalse(caught);
    }

    /** Tests that a Promise that uses its result throws on rejection if it has no handler. */
    @Test
    public void rejectPromiseNoHandler() {
        Promise<Integer> promise = new Promise<>();
        promise.then(PromiseTest.identity()).then(PromiseTest.pass());

        boolean caught = false;
        try {
            promise.reject();
            ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        } catch (UnhandledRejectionException e) {
            caught = true;
        }
        assertTrue(caught);
    }

    /** Tests that a Promise that handles rejection does not throw on rejection. */
    @Test
    public void rejectPromiseHandled() {
        Promise<Integer> promise = new Promise<>();
        promise.then(PromiseTest.identity()).then(PromiseTest.pass(), PromiseTest.pass());

        boolean caught = false;
        try {
            promise.reject();
            ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        } catch (UnhandledRejectionException e) {
            caught = true;
        }
        assertFalse(caught);
    }

    /** Tests that rejections carry the exception information. */
    @Test
    public void rejectionInformation() {
        Promise<Integer> promise = new Promise<>();
        promise.then(PromiseTest.pass());

        String message = "Promise Test";
        try {
            promise.reject(new NegativeArraySizeException(message));
            ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
            fail();
        } catch (UnhandledRejectionException e) {
            assertTrue(e.getCause() instanceof NegativeArraySizeException);
            assertEquals(e.getCause().getMessage(), message);
        }
    }

    /** Tests that rejections propagate. */
    @Test
    public void rejectionChaining() {
        final Value value = new Value();
        Promise<Integer> promise = new Promise<>();

        Promise<Integer> result = promise.then(PromiseTest.identity()).then(PromiseTest.identity());

        result.then(PromiseTest.pass(), PromiseTest.setValue(value, 5));

        promise.reject(new Exception());
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        assertEquals(value.get(), 5);
        assertTrue(result.isRejected());
    }

    /** Tests that Promises get rejected if a Function throws. */
    @Test
    public void rejectOnThrow() {
        Value value = new Value();
        Promise<Integer> promise = new Promise<>();
        promise.then(
                        (Function)
                                unusedArg -> {
                                    throw new IllegalArgumentException();
                                })
                .then(PromiseTest.pass(), PromiseTest.setValue(value, 5));

        promise.fulfill(0);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertEquals(value.get(), 5);
    }

    /** Tests that Promises get rejected if an AsyncFunction throws. */
    @Test
    public void rejectOnAsyncThrow() {
        Value value = new Value();
        Promise<Integer> promise = new Promise<>();

        promise.then(
                        (Promise.AsyncFunction)
                                unusedArg -> {
                                    throw new IllegalArgumentException();
                                })
                .then(PromiseTest.pass(), PromiseTest.setValue(value, 5));

        promise.fulfill(0);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertEquals(value.get(), 5);
    }

    /** Tests that Promises get rejected if an AsyncFunction rejects. */
    @Test
    public void rejectOnAsyncReject() {
        Value value = new Value();
        Promise<Integer> promise = new Promise<>();
        final Promise<Integer> inner = new Promise<>();

        promise.then(unusedArg -> inner).then(PromiseTest.pass(), PromiseTest.setValue(value, 5));

        promise.fulfill(0);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertEquals(value.get(), 0);

        inner.reject();

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertEquals(value.get(), 5);
    }

    @Test
    public void andFinallyOnFulfill() {
        Value value = new Value();
        Promise<Integer> promise = new Promise<>();

        promise.andFinally(() -> value.set(5));
        assertEquals(0, value.get());

        promise.fulfill(0);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertEquals(5, value.get());
    }

    @Test
    public void andFinallyOnReject() {
        Value value = new Value();
        Promise<Integer> promise = new Promise<>();

        promise.andFinally(() -> value.set(5));
        assertEquals(0, value.get());

        promise.reject();

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertEquals(5, value.get());
    }

    @Test
    public void andFinallyChainingFulfillment() {
        Value value = new Value();
        Promise<Integer> promise = new Promise<>();

        Promise<Integer> chainedPromise =
                promise.andFinally(() -> value.set(value.get() + 1))
                        .then(Object::toString)
                        .andFinally(() -> value.set(value.get() * 10))
                        .then(String::length);
        assertEquals(0, value.get());
        assertTrue(chainedPromise.isPending());

        promise.fulfill(123);
        assertTrue(chainedPromise.isFulfilled());
        assertEquals(10, value.get());
        assertEquals(3, chainedPromise.getResult().intValue());
    }

    @Test
    public void andFinallyChainingRejection() {
        Value value = new Value();
        Promise<Integer> promise = new Promise<>();

        Promise<Integer> chainedPromise =
                promise.andFinally(() -> value.set(value.get() + 1))
                        .then(Object::toString)
                        .andFinally(() -> value.set(value.get() * 10))
                        .then(String::length);
        assertEquals(0, value.get());
        assertTrue(chainedPromise.isPending());

        promise.reject();
        assertEquals(10, value.get()); // Both `andFinally()` still run.
        assertTrue(chainedPromise.isRejected());
    }

    /** Convenience method that returns a Callback that does nothing with its result. */
    private static <T> Callback<T> pass() {
        return unusedArg -> {};
    }

    /** Convenience method that returns a Function that just passes through its argument. */
    private static <T> Function<T, T> identity() {
        return argument -> argument;
    }

    /** Convenience method that returns a Callback that sets the given Value on execution. */
    private static <T> Callback<T> setValue(final Value toSet, final int value) {
        return unusedArg -> {
            toSet.set(value);
        };
    }
}
