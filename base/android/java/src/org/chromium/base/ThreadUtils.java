// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.os.Handler;
import android.os.Looper;
import android.os.Process;

import org.jni_zero.CalledByNative;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;

import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.FutureTask;

/** Helper methods to deal with threading related tasks. */
public class ThreadUtils {

    private static final Object sLock = new Object();

    private static volatile boolean sWillOverride;

    private static volatile Handler sUiThreadHandler;

    private static boolean sThreadAssertsDisabledForTesting;

    /**
     * A helper object to ensure that interactions with a particular object only happens on a
     * particular thread.
     *
     * Example:
     * <pre>
     * {@code
     * class Foo {
     *     // Valid thread is set during construction here.
     *     private final ThreadChecker mThreadChecker = new ThreadChecker();
     *
     *     public void doFoo() {
     *         mThreadChecker.assertOnValidThread();
     *     }
     * }
     * }
     * </pre>
     */
    public static class ThreadChecker {
        private final long mThreadId = Process.myTid();

        /**
         * Asserts that the current thread is the same as the one the ThreadChecker was constructed
         * on.
         */
        public void assertOnValidThread() {
            assert sThreadAssertsDisabledForTesting || mThreadId == Process.myTid()
                    : "Must only be used on a single thread.";
        }
    }

    public static void setWillOverrideUiThread() {
        sWillOverride = true;
        assert sUiThreadHandler == null;
    }

    public static void clearUiThreadForTesting() {
        sWillOverride = false;
        PostTask.resetUiThreadForTesting(); // IN-TEST
        sUiThreadHandler = null;
    }

    public static void setUiThread(Looper looper) {
        assert looper != null;
        synchronized (sLock) {
            if (sUiThreadHandler == null) {
                Handler uiThreadHandler = new Handler(looper);
                // Set up the UI Thread TaskExecutor before signaling readiness.
                PostTask.onUiThreadReady(uiThreadHandler);
                // volatile write signals readiness since other threads read it without acquiring
                // sLock.
                sUiThreadHandler = uiThreadHandler;
                // Must come after PostTask is initialized since it uses PostTask.
                TraceEvent.onUiThreadReady();
            } else if (sUiThreadHandler.getLooper() != looper) {
                throw new RuntimeException(
                        "UI thread looper is already set to "
                                + sUiThreadHandler.getLooper()
                                + " (Main thread looper is "
                                + Looper.getMainLooper()
                                + "), cannot set to new looper "
                                + looper);
            }
        }
    }

    public static Handler getUiThreadHandler() {
        if (sUiThreadHandler != null) return sUiThreadHandler;

        if (sWillOverride) {
            throw new RuntimeException("Did not yet override the UI thread");
        }
        setUiThread(Looper.getMainLooper());
        return sUiThreadHandler;
    }

    /**
     * Run the supplied Runnable on the main thread. The method will block until the Runnable
     * completes.
     *
     * Note that non-test usage of this function is heavily discouraged. For non-tests, use
     * callbacks rather than blocking threads.
     *
     * @param r The Runnable to run.
     */
    public static void runOnUiThreadBlocking(final Runnable r) {
        PostTask.runSynchronously(TaskTraits.UI_DEFAULT, r);
    }

    /**
     * Run the supplied Callable on the main thread, wrapping any exceptions in a RuntimeException.
     * The method will block until the Callable completes.
     *
     * Note that non-test usage of this function is heavily discouraged. For non-tests, use
     * callbacks rather than blocking threads.
     *
     * @param c The Callable to run
     * @return The result of the callable
     */
    public static <T> T runOnUiThreadBlockingNoException(Callable<T> c) {
        try {
            return runOnUiThreadBlocking(c);
        } catch (ExecutionException e) {
            throw new RuntimeException("Error occurred waiting for callable", e);
        }
    }

    /**
     * Run the supplied Callable on the main thread, The method will block until the Callable
     * completes.
     *
     * Note that non-test usage of this function is heavily discouraged. For non-tests, use
     * callbacks rather than blocking threads.
     *
     * @param c The Callable to run
     * @return The result of the callable
     * @throws ExecutionException c's exception
     */
    public static <T> T runOnUiThreadBlocking(Callable<T> c) throws ExecutionException {
        return PostTask.runSynchronously(TaskTraits.UI_DEFAULT, c);
    }

    /**
     * Run the supplied FutureTask on the main thread. The method will block only if the current
     * thread is the main thread.
     *
     * @param task The FutureTask to run
     * @return The queried task (to aid inline construction)
     */
    public static <T> FutureTask<T> runOnUiThread(FutureTask<T> task) {
        PostTask.runOrPostTask(TaskTraits.UI_DEFAULT, task);
        return task;
    }

    /**
     * Run the supplied Callable on the main thread. The method will block only if the current
     * thread is the main thread.
     *
     * @param c The Callable to run
     * @return A FutureTask wrapping the callable to retrieve results
     */
    public static <T> FutureTask<T> runOnUiThread(Callable<T> c) {
        return runOnUiThread(new FutureTask<T>(c));
    }

    /**
     * Run the supplied Runnable on the main thread. The method will block only if the current
     * thread is the main thread.
     *
     * @param r The Runnable to run
     */
    public static void runOnUiThread(Runnable r) {
        PostTask.runOrPostTask(TaskTraits.UI_DEFAULT, r);
    }

    /**
     * Post the supplied FutureTask to run on the main thread. The method will not block, even if
     * called on the UI thread.
     *
     * @param task The FutureTask to run
     * @return The queried task (to aid inline construction)
     */
    public static <T> FutureTask<T> postOnUiThread(FutureTask<T> task) {
        PostTask.postTask(TaskTraits.UI_DEFAULT, task);
        return task;
    }

    /**
     * Post the supplied Runnable to run on the main thread. The method will not block, even if
     * called on the UI thread.
     *
     * @param r The Runnable to run
     */
    public static void postOnUiThread(Runnable r) {
        PostTask.postTask(TaskTraits.UI_DEFAULT, r);
    }

    /**
     * Post the supplied Runnable to run on the main thread after the given amount of time. The
     * method will not block, even if called on the UI thread.
     *
     * @param r The Runnable to run
     * @param delayMillis The delay in milliseconds until the Runnable will be run
     */
    public static void postOnUiThreadDelayed(Runnable r, long delayMillis) {
        PostTask.postDelayedTask(TaskTraits.UI_DEFAULT, r, delayMillis);
    }

    /**
     * Throw an exception (when DCHECKs are enabled) if currently not running on the UI thread.
     *
     * Can be disabled by setThreadAssertsDisabledForTesting(true).
     */
    public static void assertOnUiThread() {
        if (sThreadAssertsDisabledForTesting) return;

        assert runningOnUiThread() : "Must be called on the UI thread.";
    }

    /**
     * Throw an exception (regardless of build) if currently not running on the UI thread.
     *
     * Can be disabled by setThreadAssertsEnabledForTesting(false).
     *
     * @see #assertOnUiThread()
     */
    public static void checkUiThread() {
        if (!sThreadAssertsDisabledForTesting && !runningOnUiThread()) {
            throw new IllegalStateException("Must be called on the UI thread.");
        }
    }

    /**
     * Throw an exception (when DCHECKs are enabled) if currently running on the UI thread.
     *
     * Can be disabled by setThreadAssertsDisabledForTesting(true).
     */
    public static void assertOnBackgroundThread() {
        if (sThreadAssertsDisabledForTesting) return;

        assert !runningOnUiThread() : "Must be called on a thread other than UI.";
    }

    /**
     * Disables thread asserts.
     *
     * Can be used by tests where code that normally runs multi-threaded is going to run
     * single-threaded for the test (otherwise asserts that are valid in production would fail in
     * those tests).
     */
    public static void setThreadAssertsDisabledForTesting(boolean disabled) {
        sThreadAssertsDisabledForTesting = disabled;
        ResettersForTesting.register(() -> sThreadAssertsDisabledForTesting = false);
    }

    /**
     * @return true iff the current thread is the main (UI) thread.
     */
    public static boolean runningOnUiThread() {
        return getUiThreadHandler().getLooper() == Looper.myLooper();
    }

    public static Looper getUiThreadLooper() {
        return getUiThreadHandler().getLooper();
    }

    /** Set thread priority to audio. */
    @CalledByNative
    public static void setThreadPriorityAudio(int tid) {
        Process.setThreadPriority(tid, Process.THREAD_PRIORITY_AUDIO);
    }

    /**
     * Checks whether Thread priority is THREAD_PRIORITY_AUDIO or not.
     * @param tid Thread id.
     * @return true for THREAD_PRIORITY_AUDIO and false otherwise.
     */
    @CalledByNative
    private static boolean isThreadPriorityAudio(int tid) {
        return Process.getThreadPriority(tid) == Process.THREAD_PRIORITY_AUDIO;
    }
}
