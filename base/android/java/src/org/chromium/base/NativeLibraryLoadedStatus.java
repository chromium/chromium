// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.chromium.build.BuildConfig;

/**
 * Exposes native library loading status.
 */
public class NativeLibraryLoadedStatus {
    /**
     * Interface for querying native method availability.
     */
    public interface NativeLibraryLoadedStatusProvider {
        boolean areMainDexNativeMethodsReady();
        boolean areNativeMethodsReady();
    }

    private static NativeLibraryLoadedStatusProvider sProvider;

    public static void checkLoaded(boolean isMainDex) {
        // Necessary to make sure all of these calls are stripped in release builds.
        if (!BuildConfig.ENABLE_ASSERTS) return;

        if (sProvider == null) return;

        boolean nativeMethodsReady = isMainDex ? sProvider.areMainDexNativeMethodsReady()
                                               : sProvider.areNativeMethodsReady();
        if (!nativeMethodsReady) {
            throw new JniException("Native method called before the native library was ready.");
        }
    }

    public static void setProvider(NativeLibraryLoadedStatusProvider statusProvider) {
        sProvider = statusProvider;
    }

    public static NativeLibraryLoadedStatusProvider getProviderForTesting() {
        return sProvider;
    }
}
