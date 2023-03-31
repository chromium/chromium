// Copyright 2019 The Chromium Authors
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
        boolean areNativeMethodsReady();
    }

    private static NativeLibraryLoadedStatusProvider sProvider;

    public static void checkLoaded() {
        // Necessary to make sure all of these calls are stripped in release builds.
        if (!BuildConfig.ENABLE_ASSERTS) return;

        if (sProvider == null) return;

        if (!sProvider.areNativeMethodsReady()) {
            throw new JniException(
                    String.format("Native method called before the native library was ready."));
        }
    }

    public static void setProvider(NativeLibraryLoadedStatusProvider statusProvider) {
        sProvider = statusProvider;
    }

    public static NativeLibraryLoadedStatusProvider getProviderForTesting() {
        return sProvider;
    }
}
