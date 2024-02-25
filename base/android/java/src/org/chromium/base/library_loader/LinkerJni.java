// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.library_loader;

import androidx.annotation.NonNull;

/**
 * Mockable stub for all native methods in Linker.
 *
 * This functionality is usually generated from @NativeMethods, which cannot be used for the
 * auxiliary native library used by classes in Linker and other classes in this package.
 */
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

    // Does not use JNI Generator because the native side is in libchromium_linker.so rather
    // libmonochrome.so
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
}
