// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.library_loader;

import androidx.annotation.NonNull;

import org.chromium.base.annotations.CalledByNative;
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
    public void findMemoryRegionAtRandomAddress(@NonNull Linker.LibInfo libInfo) {
        nativeFindMemoryRegionAtRandomAddress(libInfo);
    }

    @Override
    public void reserveMemoryForLibrary(@NonNull Linker.LibInfo libInfo) {
        nativeReserveMemoryForLibrary(libInfo);
    }

    @Override
    public boolean findRegionReservedByWebViewZygote(@NonNull Linker.LibInfo libInfo) {
        return nativeFindRegionReservedByWebViewZygote(libInfo);
    }

    @Override
    public boolean loadLibrary(
            String libFilePath, Linker.LibInfo libInfo, boolean spawnRelroRegion) {
        return nativeLoadLibrary(libFilePath, libInfo, spawnRelroRegion);
    }

    @Override
    public boolean useRelros(long localLoadAddress, Linker.LibInfo remoteLibInfo) {
        return nativeUseRelros(localLoadAddress, remoteLibInfo);
    }

    @Override
    public int getRelroSharingResult() {
        return nativeGetRelroSharingResult();
    }

    private static native void nativeFindMemoryRegionAtRandomAddress(
            @NonNull Linker.LibInfo libInfo);
    private static native void nativeReserveMemoryForLibrary(@NonNull Linker.LibInfo libInfo);
    private static native boolean nativeFindRegionReservedByWebViewZygote(
            @NonNull Linker.LibInfo libInfo);
    private static native boolean nativeLoadLibrary(
            String libFilePath, Linker.LibInfo libInfo, boolean spawnRelroRegion);
    private static native boolean nativeUseRelros(
            long localLoadAddress, Linker.LibInfo remoteLibInfo);
    private static native int nativeGetRelroSharingResult();

    @CalledByNative
    public static void reportDlopenExtTime(long millis) {
        Linker.reportDlopenExtTime(millis);
    }

    @CalledByNative
    public static void reportIteratePhdrTime(long millis) {
        Linker.reportIteratePhdrTime(millis);
    }
}
