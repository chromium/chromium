// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.library_loader;

import androidx.annotation.NonNull;

import org.chromium.base.annotations.JniIgnoreNatives;

/**
 * Mockable stub for all native methods in Linker.
 *
 * This functionality is usually generated from @NativeMethods, which cannot be used for the
 * auxiliary native library used by classes in Linker and other classes in this package.
 *
 * Generation of JNI stubs for classes in this package is omitted via @JniIgnoreNatives because
 * otherwise the generated native parts would have been linked into lib{,mono}chrome.so instead of
 * lib$LINKER_JNI_LIBRARY.so, where they are needed.
 */
@JniIgnoreNatives
class LinkerJni implements Linker.Natives {
    @Override
    public void findMemoryRegionAtRandomAddress(
            @NonNull Linker.LibInfo libInfo, boolean keepReserved) {
        nativeFindMemoryRegionAtRandomAddress(libInfo, keepReserved);
    }

    @Override
    public void reserveMemoryForLibrary(@NonNull Linker.LibInfo libInfo) {
        nativeReserveMemoryForLibrary(libInfo);
    }

    @Override
    public boolean findRegionReservedByWebViewZygote(@NonNull Linker.LibInfo libInfo) {
        return nativeFindRegionReservedByWebViewZygote(libInfo);
    }

    private static native void nativeFindMemoryRegionAtRandomAddress(
            @NonNull Linker.LibInfo libInfo, boolean keepReserved);
    private static native void nativeReserveMemoryForLibrary(@NonNull Linker.LibInfo libInfo);
    private static native boolean nativeFindRegionReservedByWebViewZygote(
            @NonNull Linker.LibInfo libInfo);
}
