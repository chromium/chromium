// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import androidx.test.InstrumentationRegistry;

import org.chromium.base.StrictModeContext;

import java.io.Closeable;
import java.io.IOException;
import java.util.concurrent.Callable;

/**
 * A Closeable that manages another Closeable running on android's main thread.
 *
 * The Closeable is both created and closed on the main thread.
 * Note that both operations are synchronous.
 */
public final class CloseableOnMainThread implements Closeable {
    private Closeable mCloseable;
    private Exception mException;

    /**
     * Execute a closeable callable on the main thread, blocking until it is complete.
     *
     * @param initializer A closeable callable to be executed on the main thread
     * @throws Exception Thrown if the initializer throws Exception
     */
    public CloseableOnMainThread(Callable<Closeable> initializer) throws Exception {
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            try {
                                mCloseable = initializer.call();
                            } catch (Exception e) {
                                mException = e;
                            }
                        });
        if (mException != null) {
            throw new Exception(mException.getCause());
        }
    }

    /**
     * Close the closeable on the main thread, blocking until it is complete.
     *
     * @throws IOException Thrown if the closeable throws IOException
     */
    @Override
    public void close() throws IOException {
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            try {
                                mCloseable.close();
                            } catch (IOException e) {
                                mException = e;
                            }
                        });
        if (mException != null) {
            throw new IOException(mException.getCause());
        }
    }

    /**
     * Enables try-with-resources compatible StrictMode violation allowlisting on android's main
     * thread.
     *
     * Prefer "ignored" as the variable name to appease Android Studio's "Unused symbol" inspection.
     *
     * Example:
     * <pre>
     *     try (CloseableOnMainThread ignored =
     *                     CloseableOnMainThread.StrictMode.allowDiskWrites()) {
     *         return Example.doThingThatRequiresDiskWrites();
     *     }
     * </pre>
     *
     */
    public static class StrictMode {
        private StrictMode() {}

        /**
         * Convenience method for disabling all thread-level StrictMode checks with
         * try-with-resources. Includes everything listed here:
         *     https://developer.android.com/reference/android/os/StrictMode.ThreadPolicy.Builder.html
         */
        public static CloseableOnMainThread allowAllThreadPolicies() throws Exception {
            return new CloseableOnMainThread(
                    () -> {
                        return StrictModeContext.allowAllThreadPolicies();
                    });
        }

        /** Convenience method for disabling StrictMode for disk-writes with try-with-resources. */
        public static CloseableOnMainThread allowDiskWrites() throws Exception {
            return new CloseableOnMainThread(
                    () -> {
                        return StrictModeContext.allowDiskWrites();
                    });
        }
    }
}
