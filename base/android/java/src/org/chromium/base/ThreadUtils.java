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
import org.chromium.build.BuildConfig;

import java.util.concurrent.Callable;
import java.util.concurrent.FutureTask;

/** Helper methods to deal with threading related tasks. */
public class ThreadUtils {

    private static final Object sLock = new Object();

    private static volatile boolean sWillOverride;

    private static volatile Handler sUiThreadHandler;

    private static Throwable sUiThreadInitializer;
    private static boolean sThreadAssertsDisabledForTesting;
    private static Thread sInstrumentationThreadForTesting;

    /**
     * A helper object to ensure that interactions with a particular object only happens on a
     * particular thread.
     *
     * <pre>Example:
     *
     * class Foo {
     *     // Valid thread is set during construction here.
     *     private final ThreadChecker mThreadChecker = new ThreadChecker();
     *
     *     public void doFoo() {
     *         mThreadChecker.assertOnValidThread();
     *     }
     * }
     * </pre>
     */
    // TODO(b/274802355): Add @CheckDiscard once R8 can remove this.
    public static class ThreadChecker {
        private Thread mThread;

        public ThreadChecker() {
            resetThreadId();
        }

        public void resetThreadId() {
            if (BuildConfig.ENABLE_ASSERTS) {
                mThread = Thread.currentThread();
            }
        }

        /**
         * Asserts that the current thread is the same as the one the ThreadChecker was constructed
         * on.
         */
        public void assertOnValidThread() {
            assertOnValidThreadHelper(false);
        }

        /**
         * Asserts that the current thread is the same as the one the ThreadChecker was constructed
         * on, or the Instrumentation thread.
         */
        public void assertOnValidOrInstrumentationThread() {
            assertOnValidThreadHelper(true);
        }

        private void assertOnValidThreadHelper(boolean allowInstrThread) {
            if (BuildConfig.ENABLE_ASSERTS && !sThreadAssertsDisabledForTesting) {
                Thread curThread = Thread.currentThread();
                if (curThread == mThread
                        || (allowInstrThread && curThread == sInstrumentationThreadForTesting)) {
                    return;
                }
                Thread uiThread = getUiThreadLooper().getThread();
                if (curThread == uiThread) {
                    assert false
                            : "Background-only class called from UI thread (expected: "
                                    + mThread
                                    + ")";
                } else if (mThread == uiThread) {
                    assert false : "UI-only class called from background thread: " + curThread;
                }
                assert false
                        : "Method called from wrong background thread. Expected: "
                                + mThread
                                + " Actual: "
                                + curThread;
            }
        }
    }

    public static void setWillOverrideUiThread() {
        sWillOverride = true;
        if (BuildConfig.ENABLE_ASSERTS && sUiThreadHandler != null) {
            throw new AssertionError("UI Thread already set", sUiThreadInitializer);
        }
    }

    @SuppressWarnings("StaticAssignmentOfThrowable")
    public static void clearUiThreadForTesting() {
        sWillOverride = false;
        PostTask.resetUiThreadForTesting(); // IN-TEST
        sUiThreadHandler = null;
        sUiThreadInitializer = null;
    }

    @SuppressWarnings("StaticAssignmentOfThrowable")
    public static void setUiThread(Looper looper) {
        assert looper != null;
        synchronized (sLock) {
            if (sUiThreadHandler == null) {
                if (BuildConfig.ENABLE_ASSERTS) {
                    sUiThreadInitializer = new Throwable("This is who set sUiThreadHandler.");
                }
                Handler uiThreadHandler = new Handler(looper);
                // volatile write signals readiness since other threads read it without acquiring
                // sLock.
                sUiThreadHandler = uiThreadHandler;
                // Must come after PostTask is initialized since it uses PostTask.
                TraceEvent.onUiThreadReady();
            } else if (sUiThreadHandler.getLooper() != looper) {
                RuntimeException exception =
                        new RuntimeException(
                                "UI thread looper is already set to "
                                        + sUiThreadHandler.getLooper()
                                        + " (Main thread looper is "
                                        + Looper.getMainLooper()
                                        + "), cannot set to new looper "
                                        + looper);
                if (BuildConfig.ENABLE_ASSERTS) {
                    exception.initCause(sUiThreadInitializer);
                }
                throw exception;
            }
        }
    }

    // Allows ThreadChecker to allowlist instrumentation thread calls.
    public static void recordInstrumentationThreadForTesting() {
        assert sInstrumentationThreadForTesting == null;
        assert Looper.getMainLooper() != Looper.myLooper();
        sInstrumentationThreadForTesting = Thread.currentThread();
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
     * <p>Note that non-test usage of this function is heavily discouraged. For non-tests, use
     * callbacks rather than blocking threads.
     *
     * @param r The Runnable to run.
     */
    public static void runOnUiThreadBlocking(Runnable r) {
        PostTask.runSynchronously(TaskTraits.UI_DEFAULT, r);
    }

    /**
     * Run the supplied Callable on the main thread, The method will block until the Callable
     * completes.
     *
     * <p>Note that non-test usage of this function is heavily discouraged. For non-tests, use
     * callbacks rather than blocking threads.
     *
     * @param c The Callable to run
     * @return The result of the callable
     */
    public static <T> T runOnUiThreadBlocking(Callable<T> c) {
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
     * <p>Can be used by tests where code that normally runs multi-threaded is going to run
     * single-threaded for the test (otherwise asserts that are valid in production would fail in
     * those tests). Avoid to use this in ui tests, especially under the batch unit tests
     * environment, because any ThreadChecker instances created on the wrong thread will likely fail
     * on subsequent tests when run on their correct threads. Prefer to use `runOnUiThread()` or
     * `PostTask.runSynchronously()`.
     */
    public static void hasSubtleSideEffectsSetThreadAssertsDisabledForTesting(boolean disabled) {
        sThreadAssertsDisabledForTesting = disabled;
        ResettersForTesting.register(() -> sThreadAssertsDisabledForTesting = false);
    }

    /**
     * Disables thread asserts.
     *
     * <p>Can be used by tests where code that normally runs multi-threaded is going to run
     * single-threaded for the test (otherwise asserts that are valid in production would fail in
     * those tests).
     */
    @Deprecated
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

    /**
     * @return true iff the current thread is the instrumentation thread.
     */
    public static boolean runningOnInstrumentationThread() {
        return sInstrumentationThreadForTesting != null
                && sInstrumentationThreadForTesting == Thread.currentThread();
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
