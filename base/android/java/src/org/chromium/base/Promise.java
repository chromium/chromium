// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.os.Handler;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.LinkedList;
import java.util.List;
import java.util.function.Function;

/**
 * A Promise class to be used as a placeholder for a result that will be provided asynchronously.
 * It must only be accessed from a single thread.
 * @param <T> The type the Promise will be fulfilled with.
 */
public class Promise<T> {
    // TODO(peconn): Implement rejection handlers that can recover from rejection.

    @IntDef({PromiseState.UNFULFILLED, PromiseState.FULFILLED, PromiseState.REJECTED})
    @Retention(RetentionPolicy.SOURCE)
    private @interface PromiseState {
        int UNFULFILLED = 0;
        int FULFILLED = 1;
        int REJECTED = 2;
    }

    @PromiseState private int mState = PromiseState.UNFULFILLED;

    private T mResult;
    private final List<Callback<T>> mFulfillCallbacks = new LinkedList<>();

    private Exception mRejectReason;
    private final List<Callback<Exception>> mRejectCallbacks = new LinkedList<>();

    private final Thread mThread = Thread.currentThread();
    private final Handler mHandler = new Handler();

    private boolean mThrowingRejectionHandler;

    /**
     * A function class for use when chaining Promises with {@link Promise#then(AsyncFunction)}.
     * @param <A> The type of the function input.
     * @param <RT> The type of the function output.
     */
    public interface AsyncFunction<A, RT> extends Function<A, Promise<RT>> {}

    /**
     * An exception class for when a rejected Promise is not handled and cannot pass the rejection
     * to a subsequent Promise.
     */
    public static class UnhandledRejectionException extends RuntimeException {
        public UnhandledRejectionException(String message, Throwable cause) {
            super(message, cause);
        }
    }

    /**
     * Convenience method that calls {@link #then(Callback, Callback)} providing a rejection
     * {@link Callback} that throws a {@link UnhandledRejectionException}. Only use this on
     * Promises that do not have rejection handlers or dependant Promises.
     */
    public void then(Callback<T> onFulfill) {
        checkThread();

        // Allow multiple single argument then(Callback)'s, but don't bother adding duplicate
        // throwing rejection handlers.
        if (mThrowingRejectionHandler) {
            thenInner(onFulfill);
            return;
        }

        assert mRejectCallbacks.size() == 0
                : "Do not call the single argument Promise.then(Callback) on a Promise that already"
                        + " has a rejection handler.";

        Callback<Exception> onReject =
                reason -> {
                    throw new UnhandledRejectionException(
                            "Promise was rejected without a rejection handler.", reason);
                };

        then(onFulfill, onReject);
        mThrowingRejectionHandler = true;
    }

    /**
     * Queues {@link Callback}s to be run when the Promise is either fulfilled or rejected. If the
     * Promise is already fulfilled or rejected, the appropriate callback will be run on the next
     * iteration of the message loop.
     *
     * @param onFulfill The Callback to be called on fulfillment.
     * @param onReject The Callback to be called on rejection. The argument to onReject will
     *         may be null if the Promise was rejected manually.
     */
    public void then(Callback<T> onFulfill, Callback<Exception> onReject) {
        checkThread();
        thenInner(onFulfill);
        exceptInner(onReject);
    }

    /**
     * Adds a rejection handler to the Promise. This handler will be called if this Promise or any
     * Promises this Promise depends on is rejected or fails. The {@link Callback} will be given
     * the exception that caused the rejection, or null if the rejection was manual (caused by a
     * call to {@link #reject()}.
     */
    public void except(Callback<Exception> onReject) {
        checkThread();
        exceptInner(onReject);
    }

    private void thenInner(Callback<T> onFulfill) {
        if (mState == PromiseState.FULFILLED) {
            postCallbackToLooper(onFulfill, mResult);
        } else if (mState == PromiseState.UNFULFILLED) {
            mFulfillCallbacks.add(onFulfill);
        }
    }

    private void exceptInner(Callback<Exception> onReject) {
        assert !mThrowingRejectionHandler
                : "Do not add an exception handler to a Promise you have "
                        + "called the single argument Promise.then(Callback) on.";

        if (mState == PromiseState.REJECTED) {
            postCallbackToLooper(onReject, mRejectReason);
        } else if (mState == PromiseState.UNFULFILLED) {
            mRejectCallbacks.add(onReject);
        }
    }

    /**
     * Queues a {@link Function} to be run when the Promise is fulfilled. When this Promise is
     * fulfilled, the function will be run and its result will be place in the returned Promise.
     */
    public <RT> Promise<RT> then(final Function<T, RT> function) {
        checkThread();

        // Create a new Promise to store the result of the function.
        final Promise<RT> promise = new Promise<>();

        // Once this Promise is fulfilled:
        // - Apply the given function to the result.
        // - Fulfill the new Promise.
        thenInner(
                result -> {
                    try {
                        promise.fulfill(function.apply(result));
                    } catch (Exception e) {
                        // If function application fails, reject the next Promise.
                        promise.reject(e);
                    }
                });

        // If this Promise is rejected, reject the next Promise.
        exceptInner(promise::reject);

        return promise;
    }

    /**
     * Queues a {@link Promise.AsyncFunction} to be run when the Promise is fulfilled. When this
     * Promise is fulfilled, the AsyncFunction will be run. When the result of the AsyncFunction is
     * available, it will be placed in the returned Promise.
     */
    public <RT> Promise<RT> then(final AsyncFunction<T, RT> function) {
        checkThread();

        // Create a new Promise to be returned.
        final Promise<RT> promise = new Promise<>();

        // Once this Promise is fulfilled:
        // - Apply the given function to the result (giving us an inner Promise).
        // - On fulfillment of this inner Promise, fulfill our return Promise.
        thenInner(
                result -> {
                    try {
                        // When the inner Promise is fulfilled, fulfill the return Promise.
                        // Alternatively, if the inner Promise is rejected, reject the return
                        // Promise.
                        function.apply(result).then(promise::fulfill, promise::reject);
                    } catch (Exception e) {
                        // If creating the inner Promise failed, reject the next Promise.
                        promise.reject(e);
                    }
                });

        // If this Promise is rejected, reject the next Promise.
        exceptInner(promise::reject);

        return promise;
    }

    /**
     * Queues a {@link Runnable} to be run when the Promise is fulfilled or rejected. This allow to
     * chain the promise while avoiding duplicating code in both the Promise's {@link #then} and
     * {@link #except} handlers.
     */
    @SuppressWarnings("unchecked")
    public Promise<T> andFinally(Runnable runnable) {
        Callback<?> asCallback = unused -> runnable.run();
        thenInner((Callback<T>) asCallback);
        exceptInner((Callback<Exception>) asCallback);
        return this;
    }

    /**
     * Fulfills the Promise with the result and passes it to any {@link Callback}s previously queued
     * on the next iteration of the message loop.
     */
    public void fulfill(final T result) {
        checkThread();
        assert mState == PromiseState.UNFULFILLED;

        mState = PromiseState.FULFILLED;
        mResult = result;

        for (final Callback<T> callback : mFulfillCallbacks) {
            postCallbackToLooper(callback, result);
        }

        mFulfillCallbacks.clear();
    }

    /**
     * Rejects the Promise, rejecting all those Promises that rely on it.
     *
     * This may throw an exception if a dependent Promise fails to handle the rejection, so it is
     * important to make it explicit when a Promise may be rejected, so that users of that Promise
     * know to provide rejection handling.
     */
    public void reject(final Exception reason) {
        checkThread();
        assert mState == PromiseState.UNFULFILLED;

        mState = PromiseState.REJECTED;
        mRejectReason = reason;

        for (final Callback<Exception> callback : mRejectCallbacks) {
            postCallbackToLooper(callback, reason);
        }
        mRejectCallbacks.clear();
    }

    /** Rejects a Promise, see {@link #reject(Exception)}. */
    public void reject() {
        reject(null);
    }

    /** Returns whether the promise is fulfilled. */
    public boolean isFulfilled() {
        checkThread();
        return mState == PromiseState.FULFILLED;
    }

    /** Returns whether the promise is rejected. */
    public boolean isRejected() {
        checkThread();
        return mState == PromiseState.REJECTED;
    }

    /** Returns whether the promise is in none of the fulfilled nor rejected states. */
    public boolean isPending() {
        checkThread();
        return mState == PromiseState.UNFULFILLED;
    }

    /**
     * Must be called after the promise has been fulfilled.
     *
     * @return The promised result.
     */
    public T getResult() {
        assert isFulfilled();
        return mResult;
    }

    /** Convenience method to return a Promise fulfilled with the given result. */
    public static <T> Promise<T> fulfilled(T result) {
        Promise<T> promise = new Promise<>();
        promise.fulfill(result);
        return promise;
    }

    /** Convenience method to return a rejected Promise. */
    public static <T> Promise<T> rejected() {
        Promise<T> promise = new Promise<>();
        promise.reject();
        return promise;
    }

    private void checkThread() {
        assert mThread == Thread.currentThread() : "Promise must only be used on a single Thread.";
    }

    // We use a different template parameter here so this can be used for both T and Throwables.
    private <S> void postCallbackToLooper(final Callback<S> callback, final S result) {
        // Post the callbacks to the Thread looper so we don't get a long chain of callbacks
        // holding up the thread.
        mHandler.post(callback.bind(result));
    }
}
