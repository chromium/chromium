// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Objects;

import javax.annotation.concurrent.GuardedBy;

/**
 * Class allowing to wrap lambdas, such as {@link Callback} or {@link Runnable} with a cancelable
 * version of the same, and cancel them in bulk when {@link #destroy()} is called. Use an instance
 * of this class to wrap lambdas passed to other objects, and later use {@link #destroy()} to
 * prevent future invocations of these lambdas.
 *
 * <p>Besides helping with lifecycle management, this also prevents holding onto object references
 * after callbacks have been canceled.
 *
 * <p>Example usage:
 *
 * <pre>{@code
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
 * }</pre>
 *
 * <p>It does not matter if the lambda is intended to be invoked once or more times, as it is only
 * weakly referred from this class. When the lambda is no longer needed, it can be safely garbage
 * collected. All invocations after {@link #destroy()} will be ignored.
 *
 * <p>Each instance of this class in only meant for a single {@link #destroy()} call. After it is
 * destroyed, the owning class should create a new instance instead:
 *
 * <pre>{@code
 * // Somewhere inside Foo.
 * mCallbackController.destroy();  // Invalidates all current callbacks.
 * mCallbackController = new CallbackController();  // Allows to start handing out new callbacks.
 * }</pre>
 */
@SuppressWarnings({"NoSynchronizedThisCheck", "NoSynchronizedMethodCheck"})
public final class CallbackController {
    /** Interface for cancelable objects tracked by this class. */
    private interface Cancelable {
        /** Cancels the object, preventing its execution, when triggered. */
        void cancel();
    }

    /** Class wrapping a {@link Callback} interface with a {@link Cancelable} interface. */
    private class CancelableCallback<T> implements Cancelable, Callback<T> {
        @GuardedBy("CallbackController.this")
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
            synchronized (CallbackController.this) {
                if (mCallback != null) mCallback.onResult(result);
            }
        }
    }

    /** Class wrapping {@link Runnable} interface with a {@link Cancelable} interface. */
    private class CancelableRunnable implements Cancelable, Runnable {
        @GuardedBy("CallbackController.this")
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
            synchronized (CallbackController.this) {
                if (mRunnable != null) mRunnable.run();
            }
        }
    }

    /** A list of cancelables created and cancelable by this object. */
    @Nullable
    @GuardedBy("this")
    private ArrayList<WeakReference<Cancelable>> mCancelables = new ArrayList<>();

    /**
     * Wraps a provided {@link Callback} with a cancelable object that is tracked by this {@link
     * CallbackController}. To cancel a resulting wrapped instance destroy the host.
     *
     * <p>This method must not be called after {@link #destroy()}.
     *
     * @param <T> The type of the callback result.
     * @param callback A callback that will be made cancelable.
     * @return A cancelable instance of the callback.
     */
    public synchronized <T> Callback<T> makeCancelable(@NonNull Callback<T> callback) {
        checkNotCanceled();
        CancelableCallback<T> cancelable = new CancelableCallback<>(callback);
        addInternal(cancelable);
        return cancelable;
    }

    /**
     * Wraps a provided {@link Runnable} with a cancelable object that is tracked by this {@link
     * CallbackController}. To cancel a resulting wrapped instance destroy the host.
     *
     * <p>This method must not be called after {@link #destroy()}.
     *
     * @param runnable A runnable that will be made cancelable.
     * @return A cancelable instance of the runnable.
     */
    public synchronized Runnable makeCancelable(@NonNull Runnable runnable) {
        checkNotCanceled();
        CancelableRunnable cancelable = new CancelableRunnable(runnable);
        addInternal(cancelable);
        return cancelable;
    }

    @GuardedBy("this")
    private void addInternal(Cancelable cancelable) {
        var cancelables = mCancelables;
        cancelables.add(new WeakReference<>(cancelable));
        // Flush null entries.
        if ((cancelables.size() % 1024) == 0) {
            // This removes null entries as a side-effect.
            // Cloning the list is inefficient, but this should rarely be hit.
            CollectionUtil.strengthen(cancelables);
        }
    }

    /**
     * Cancels all of the cancelables that have not been garbage collected yet.
     *
     * <p>This method must only be called once and makes the instance unusable afterwards.
     */
    public synchronized void destroy() {
        checkNotCanceled();
        for (Cancelable cancelable : CollectionUtil.strengthen(mCancelables)) {
            cancelable.cancel();
        }
        mCancelables = null;
    }

    /** If the cancelation already happened, throws an {@link IllegalStateException}. */
    @GuardedBy("this")
    private void checkNotCanceled() {
        // Use NullPointerException because it optimizes well.
        Objects.requireNonNull(mCancelables);
    }
}
