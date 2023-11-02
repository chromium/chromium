// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.library_loader;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JniIgnoreNatives;

/**
 * Mockable stub for all native methods in ModernLinker.
 *
 * See LinkerJni.java for an explanation of why @JniIgnoreNatives is needed.
 */
@JniIgnoreNatives
class ModernLinkerJni implements ModernLinker.Natives {
    private static final String TAG = "ModernLinkerJni";

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

    private static native boolean nativeLoadLibrary(
            String libFilePath, Linker.LibInfo libInfo, boolean spawnRelroRegion);
    private static native boolean nativeUseRelros(
            long localLoadAddress, Linker.LibInfo remoteLibInfo);
    private static native int nativeGetRelroSharingResult();

    @CalledByNative
    public static void reportDlopenExtTime(long millis) {
        ModernLinker.reportDlopenExtTime(millis);
    }

    @CalledByNative
    public static void reportIteratePhdrTime(long millis) {
        ModernLinker.reportIteratePhdrTime(millis);
    }
}
