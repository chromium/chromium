// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.library_loader;

/**
 * This is interface to preload the native library before calling System.loadLibrary.
 *
 * Preloading shouldn't call System.loadLibrary() or otherwise cause any Chromium
 * code to be run, because it can be called before Chromium command line is known.
 * It can however open the library via dlopen() or android_dlopen_ext() so that
 * dlopen() later called by System.loadLibrary() becomes a noop. This is what the
 * only subclass (WebViewLibraryPreloader) is doing.
 */
public abstract class NativeLibraryPreloader {
    public abstract int loadLibrary(String packageName);
}
