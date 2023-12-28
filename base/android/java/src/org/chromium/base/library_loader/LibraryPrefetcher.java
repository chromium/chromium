// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.library_loader;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.SysUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Handles native library prefetch.
 *
 * See also base/android/library_loader/library_prefetcher_hooks.cc, which contains
 * the native counterpart to this class.
 */
@JNINamespace("base::android")
public class LibraryPrefetcher {

    private static final String TAG = "LibraryPrefetcher";
    // One-way switch that becomes true once {@link asyncPrefetchLibrariesToMemory} has been called.
    private static final AtomicBoolean sPrefetchLibraryHasBeenCalled = new AtomicBoolean();

    /**
     * Prefetches the native libraries in a background thread.
     *
     * Launches a task that, through a short-lived forked process, reads a
     * part of each page of the native library.  This is done to warm up the
     * page cache, turning hard page faults into soft ones.
     *
     * This is done this way, as testing shows that fadvise(FADV_WILLNEED) is
     * detrimental to the startup time.
     */
    public static void asyncPrefetchLibrariesToMemory() {
        SysUtils.logPageFaultCountToTracing();

        final boolean coldStart = sPrefetchLibraryHasBeenCalled.compareAndSet(false, true);
        // Collection should start close to the native library load, but doesn't have
        // to be simultaneous with it. Also, don't prefetch in this case, as this would
        // skew the results.
        if (coldStart && CommandLine.getInstance().hasSwitch("log-native-library-residency")) {
            // LibraryPrefetcherJni.get().periodicallyCollectResidency() sleeps, run it on another
            // thread, and not on the thread pool.
            new Thread(() -> LibraryPrefetcherJni.get().periodicallyCollectResidency()).start();
            return;
        }

        PostTask.postTask(
                TaskTraits.USER_BLOCKING,
                () -> {
                    int percentage =
                            LibraryPrefetcherJni.get().percentageOfResidentNativeLibraryCode();
                    try (TraceEvent e =
                            TraceEvent.scoped(
                                    "LibraryPrefetcher.asyncPrefetchLibrariesToMemory",
                                    Integer.toString(percentage))) {
                        // Arbitrary percentage threshold. If most of the native library is already
                        // resident (likely with monochrome), don't bother creating a prefetch
                        // process.
                        boolean prefetch = coldStart && percentage < 90;
                        if (prefetch) LibraryPrefetcherJni.get().forkAndPrefetchNativeLibrary();
                    }
                    // Removes a dead flag, don't remove the removal code before M77 at least.
                    ContextUtils.getAppSharedPreferences()
                            .edit()
                            .remove("dont_prefetch_libraries")
                            .apply();
                });
    }

    @NativeMethods
    interface Natives {
        // Finds the ranges corresponding to the native library pages, forks a new
        // process to prefetch these pages and waits for it. The new process then
        // terminates. This is blocking.
        void forkAndPrefetchNativeLibrary();

        // Returns the percentage of the native library code page that are currently reseident in
        // memory.
        int percentageOfResidentNativeLibraryCode();

        // Periodically logs native library residency from this thread.
        void periodicallyCollectResidency();
    }
}
