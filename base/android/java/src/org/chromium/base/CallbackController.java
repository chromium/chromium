// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

import javax.annotation.concurrent.GuardedBy;

/**
 * Class allowing to wrap lambdas, such as {@link Callback} or {@link Runnable} with a cancelable
 * version of the same, and cancel them in bulk when {@link #destroy()} is called. Use an instance
 * of this class to wrap lambdas passed to other objects, and later use {@link #destroy()} to
 * prevent future invocations of these lambdas.
 * <p>
 * Example usage:
 * {@code
 * public class Foo {
 *    private CallbackController mCallbackController = new CallbackController();
 *    private SomeDestructibleClass mDestructible = new SomeDestructibleClass();
 *
 *    // Classic destroy, with clean up of cancelables.
 *    public void destroy() {
 *        // This call makes sure all tracked lambdas are destroyed.
 *        // It is recommended to be done at the top of the destroy methods, to ensure calls from
 *        // other threads don't use already destroyed resources.
 *        if (mCallbackController != null) {
 *            mCallbackController.destroy();
 *            mCallbackController = null;
 *        }
 *
 *        if (mDestructible != null) {
 *            mDestructible.destroy();
 *            mDestructible = null;
 *        }
 *    }
 *
 *    // Sets up Bar instance by providing it with a set of dangerous callbacks all of which could
 *    // cause a NullPointerException if invoked after destroy().
 *    public void setUpBar(Bar bar) {
 *        // Notice all callbacks below would fail post destroy, if they were not canceled.
 *        bar.setDangerousLambda(mCallbackController.makeCancelable(() -> mDestructible.method()));
 *        bar.setDangerousRunnable(mCallbackController.makeCancelable(this::dangerousRunnable));
 *        bar.setDangerousOtherCallback(
 *                mCallbackController.makeCancelable(baz -> mDestructible.setBaz(baz)));
 *        bar.setDangerousCallback(mCallbackController.makeCancelable(this::setBaz));
 *    }
 *
 *    private void dangerousRunnable() {
 *        mDestructible.method();
 *    }
 *
 *    private void setBaz(Baz baz) {
 *        mDestructible.setBaz(baz);
 *    }
 * }
 * }
 * <p>
 * It does not matter if the lambda is intended to be invoked once or more times, as it is only
 * weakly referred from this class. When the lambda is no longer needed, it can be safely garbage
 * collected. All invocations after {@link #destroy()} will be ignored.
 * <p>
 * Each instance of this class in only meant for a single {@link
 * #destroy()} call. After it is destroyed, the owning class should create a new instance instead:
 * {@code
 *    // Somewhere inside Foo.
 *    mCallbackController.destroy();  // Invalidates all current callbacks.
 *    mCallbackController = new CallbackController();  // Allows to start handing out new callbacks.
 * }
 */
public final class CallbackController {
    /** Interface for cancelable objects tracked by this class. */
    private interface Cancelable {
        /** Cancels the object, preventing its execution, when triggered. */
        void cancel();
    }

    /** Class wrapping a {@link Callback} interface with a {@link Cancelable} interface. */
    private class CancelableCallback<T> implements Cancelable, Callback<T> {
        @GuardedBy("mReentrantLock")
        private Callback<T> mCallback;

        private CancelableCallback(@NonNull Callback<T> callback) {
            mCallback = callback;
        }

        @Override
        @SuppressWarnings("GuardedBy")
        public void cancel() {
            mCallback = null;
        }

        @Override
        public void onResult(T result) {
            // Guarantees the cancelation is not going to happen, while callback is executed by
            // another thread.
            try (AutoCloseableLock acl = AutoCloseableLock.lock(mReentrantLock)) {
                if (mCallback != null) mCallback.onResult(result);
            }
        }
    }

    /** Class wrapping {@link Runnable} interface with a {@link Cancelable} interface. */
    private class CancelableRunnable implements Cancelable, Runnable {
        @GuardedBy("mReentrantLock")
        private Runnable mRunnable;

        private CancelableRunnable(@NonNull Runnable runnable) {
            mRunnable = runnable;
        }

        @Override
        @SuppressWarnings("GuardedBy")
        public void cancel() {
            mRunnable = null;
        }

        @Override
        public void run() {
            // Guarantees the cancelation is not going to happen, while runnable is executed by
            // another thread.
            try (AutoCloseableLock acl = AutoCloseableLock.lock(mReentrantLock)) {
                if (mRunnable != null) mRunnable.run();
            }
        }
    }

    /** Class wrapping the locking logic to reduce repetitive code. */
    private static class AutoCloseableLock implements AutoCloseable {
        private final Lock mLock;
        private boolean mIsLocked;

        private AutoCloseableLock(Lock lock, boolean isLocked) {
            mLock = lock;
            mIsLocked = isLocked;
        }

        static AutoCloseableLock lock(Lock l) {
            l.lock();
            return new AutoCloseableLock(l, true);
        }

        @Override
        public void close() {
            if (!mIsLocked) throw new IllegalStateException("mLock isn't locked.");
            mIsLocked = false;
            mLock.unlock();
        }
    }

    /** A list of cancelables created and cancelable by this object. */
    @Nullable
    @GuardedBy("mReentrantLock")
    private ArrayList<WeakReference<Cancelable>> mCancelables = new ArrayList<>();

    /** Ensures thread safety of creating cancelables and canceling them. */
    private final ReentrantLock mReentrantLock = new ReentrantLock(/*fair=*/true);

    /**
     * Wraps a provided {@link Callback} with a cancelable object that is tracked by this
     * {@link CallbackController}. To cancel a resulting wrapped instance destroy the host.
     * <p>
     * This method must not be called after {@link #destroy()}.
     *
     * @param <T> The type of the callback result.
     * @param callback A callback that will be made cancelable.
     * @return A cancelable instance of the callback.
     */
    public <T> Callback<T> makeCancelable(@NonNull Callback<T> callback) {
        try (AutoCloseableLock acl = AutoCloseableLock.lock(mReentrantLock)) {
            checkNotCanceled();
            CancelableCallback<T> cancelable = new CancelableCallback<>(callback);
            mCancelables.add(new WeakReference<>(cancelable));
            return cancelable;
        }
    }

    /**
     * Wraps a provided {@link Runnable} with a cancelable object that is tracked by this
     * {@link CallbackController}. To cancel a resulting wrapped instance destroy the host.
     * <p>
     * This method must not be called after {@link #destroy()}.
     *
     * @param runnable A runnable that will be made cancelable.
     * @return A cancelable instance of the runnable.
     */
    public Runnable makeCancelable(@NonNull Runnable runnable) {
        try (AutoCloseableLock acl = AutoCloseableLock.lock(mReentrantLock)) {
            checkNotCanceled();
            CancelableRunnable cancelable = new CancelableRunnable(runnable);
            mCancelables.add(new WeakReference<>(cancelable));
            return cancelable;
        }
    }

    /**
     * Cancels all of the cancelables that have not been garbage collected yet.
     * <p>
     * This method must only be called once and makes the instance unusable afterwards.
     */
    public void destroy() {
        // This is likely an invalid state. A callback is currently being run, which means the
        // owning class using this controller is likely in the middle of a method call. But if
        // destroy is called on the controller, it is also likely being called on the owning class.
        // After destroy completes, the owning class will be in invalid state to be used, and the
        // rest of the method call will be operating with the owning class in an invalid state. If
        // something has a valid use case for this, remove this assert.
        assert !mReentrantLock.isHeldByCurrentThread();

        try (AutoCloseableLock acl = AutoCloseableLock.lock(mReentrantLock)) {
            checkNotCanceled();
            for (Cancelable cancelable : CollectionUtil.strengthen(mCancelables)) {
                cancelable.cancel();
            }
            mCancelables = null;
        }
    }

    /** If the cancelation already happened, throws an {@link IllegalStateException}. */
    @GuardedBy("mReentrantLock")
    private void checkNotCanceled() {
        if (mCancelables == null) {
            throw new IllegalStateException("This CallbackController has already been destroyed.");
        }
    }
}
