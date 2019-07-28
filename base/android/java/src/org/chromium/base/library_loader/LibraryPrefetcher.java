// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.library_loader;

import android.annotation.SuppressLint;
import android.content.Context;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.SysUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.MainDex;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Handles native library prefetch.
 *
 * See also base/android/library_loader/library_prefetcher_hooks.cc, which contains
 * the native counterpart to this class.
 */
@MainDex
@JNINamespace("base::android")
public class LibraryPrefetcher {
    /**
     * Used to pass ordered code info back from native.
     */
    private final static class OrderedCodeInfo {
        public final String filename;
        public final long startOffset;
        public final long length;

        @CalledByNative("OrderedCodeInfo")
        public OrderedCodeInfo(String filename, long startOffset, long length) {
            this.filename = filename;
            this.startOffset = startOffset;
            this.length = length;
        }

        @Override
        public String toString() {
            return "filename = " + filename + " startOffset = " + startOffset
                    + " length = " + length;
        }
    }

    private static final String TAG = "LibraryPrefetcher";
    // One-way switch that becomes true once
    // {@link asyncPrefetchLibrariesToMemory} has been called.
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
            // nativePeriodicallyCollectResidency() sleeps, run it on another thread,
            // and not on the thread pool.
            new Thread(LibraryPrefetcher::nativePeriodicallyCollectResidency).start();
            return;
        }

        PostTask.postTask(TaskTraits.USER_BLOCKING, () -> {
            int percentage = nativePercentageOfResidentNativeLibraryCode();
            try (TraceEvent e =
                            TraceEvent.scoped("LibraryPrefetcher.asyncPrefetchLibrariesToMemory",
                                    Integer.toString(percentage))) {
                // Arbitrary percentage threshold. If most of the native library is already
                // resident (likely with monochrome), don't bother creating a prefetch process.
                boolean prefetch = coldStart && percentage < 90;
                if (prefetch) nativeForkAndPrefetchNativeLibrary();
                if (percentage != -1) {
                    String histogram = "LibraryLoader.PercentageOfResidentCodeBeforePrefetch"
                            + (coldStart ? ".ColdStartup" : ".WarmStartup");
                    RecordHistogram.recordPercentageHistogram(histogram, percentage);
                }
            }
            // Removes a dead flag, don't remove the removal code before M77 at least.
            ContextUtils.getAppSharedPreferences().edit().remove("dont_prefetch_libraries").apply();
        });
    }

    // May pin Chrome's code to memory, on platforms that support it.
    @SuppressLint("WrongConstant")
    public static void maybePinOrderedCodeInMemory() {
        try (TraceEvent e = TraceEvent.scoped("LibraryPrefetcher::maybePinOrderedCodeInMemory")) {
            OrderedCodeInfo info = nativeGetOrderedCodeInfo();
            if (info == null) return;
            TraceEvent.instant("pinOrderedCodeInMemory", info.toString());

            Context context = ContextUtils.getApplicationContext();
            Object pinner = context.getSystemService("pinner");
            if (pinner == null) {
                Log.w(TAG, "Cannot get PinnerService.");
                return;
            }

            // Reflection is required because the method is neither visible in the platform, nor
            // available everywhere.
            try {
                Method pinRangeFromFile = pinner.getClass().getMethod(
                        "pinRangeFromFile", String.class, int.class, int.class);
                boolean ok = (Boolean) pinRangeFromFile.invoke(
                        pinner, info.filename, (int) info.startOffset, (int) info.length);
                if (!ok) {
                    Log.e(TAG, "Not allowed to call the method, should not happen");
                } else {
                    Log.i(TAG, "Successfully pinned ordered code");
                }
            } catch (
                    Exception ex) {
                Log.w(TAG, "Error invoking the method. " + ex.getMessage());
            }
        }
    }

    // Finds the ranges corresponding to the native library pages, forks a new
    // process to prefetch these pages and waits for it. The new process then
    // terminates. This is blocking.
    private static native void nativeForkAndPrefetchNativeLibrary();

    // Returns the percentage of the native library code page that are currently reseident in
    // memory.
    private static native int nativePercentageOfResidentNativeLibraryCode();

    // Periodically logs native library residency from this thread.
    private static native void nativePeriodicallyCollectResidency();

    // Returns the range within a file of the ordered code section, or null if this is not
    // available.
    private static native OrderedCodeInfo nativeGetOrderedCodeInfo();
}
