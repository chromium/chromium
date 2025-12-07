// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.library_loader;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;

/**
 * Handles native library prefetch.
 *
 * <p>See also base/android/library_loader/library_prefetcher_hooks.cc, which contains the native
 * counterpart to this class.
 */
@NullMarked
@JNINamespace("base::android")
public class LibraryPrefetcher {

    public static void prefetchNativeLibraryForWebView() {
        LibraryPrefetcherJni.get().prefetchNativeLibraryForWebView();
    }

    @NativeMethods
    interface Natives {
        // Finds the ranges corresponding to the native library pages, forks a new
        // process to prefetch these pages and waits for it. The new process then
        // terminates. This is blocking.
        void prefetchNativeLibraryForWebView();
    }
}
