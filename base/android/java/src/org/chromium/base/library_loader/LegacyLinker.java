// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.library_loader;

import androidx.annotation.NonNull;

import org.chromium.base.Log;
import org.chromium.base.annotations.JniIgnoreNatives;

import javax.annotation.concurrent.GuardedBy;

/**
 * Provides a concrete implementation of the Chromium Linker.
 *
 * This Linker implementation uses the crazy linker to map and then run Chrome for Android.
 *
 * For more on the operations performed by the Linker, see {@link Linker}.
 */
@JniIgnoreNatives
class LegacyLinker extends Linker {
    private static final String TAG = "LegacyLinker";

    LegacyLinker() {}

    @Override
    void setApkFilePath(String path) {
        ensureInitializedImplicitlyAsLastResort();
        synchronized (mLock) {
            nativeAddZipArchivePath(path);
        }
    }

    @Override
    protected boolean keepMemoryReservationUntilLoad() {
        // The crazylinker attempts to reserve the address range. There is a feature to load on top
        // of a reserved memory region, but it has not been tested recently, and looks buggy.
        return false;
    }

    @Override
    @GuardedBy("mLock")
    protected void loadLibraryImplLocked(String library, @RelroSharingMode int relroMode) {
        assert mState == State.INITIALIZED; // Only one successful call.

        String libFilePath = System.mapLibraryName(library);
        if (!nativeLoadLibrary(libFilePath, mLocalLibInfo.mLoadAddress, mLocalLibInfo)) {
            String errorMessage = "Unable to load library: " + libFilePath;
            Log.e(TAG, errorMessage);
            throw new UnsatisfiedLinkError(errorMessage);
        }
        mLocalLibInfo.mLibFilePath = libFilePath;

        if (relroMode == RelroSharingMode.PRODUCE || relroMode == RelroSharingMode.NO_SHARING) {
            if (!nativeCreateSharedRelro(libFilePath, mLocalLibInfo.mLoadAddress, mLocalLibInfo)) {
                Log.w(TAG, "Could not create shared RELRO for %s at %x", libFilePath,
                        mLocalLibInfo.mLoadAddress);
                // Next state is still to provide RELRO (even though there is none), as child
                // processes would wait for them.
                mLocalLibInfo.mRelroFd = -1;
            } else {
                if (DEBUG) {
                    Log.i(TAG, "Created shared RELRO for %s at 0x%x: %s", libFilePath,
                            mLocalLibInfo.mLoadAddress, mLocalLibInfo.toString());
                }
            }
            useSharedRelrosLocked(mLocalLibInfo);
            mState = State.DONE_PROVIDE_RELRO;
        } else {
            assert relroMode == RelroSharingMode.CONSUME;
            waitForSharedRelrosLocked();
            assert libFilePath.equals(mRemoteLibInfo.mLibFilePath);
            useSharedRelrosLocked(mRemoteLibInfo);
            mRemoteLibInfo.close();
            mRemoteLibInfo = null;
            mState = State.DONE;
        }
    }

    /**
     * Replace the memory mapping under RELRO with the contents of the given shared memory region.
     *
     * @param info Object containing the RELRO FD.
     */
    private static void useSharedRelrosLocked(LibInfo info) {
        String libFilePath = info.mLibFilePath;
        if (!nativeUseSharedRelro(libFilePath, info)) {
            Log.w(TAG, "Could not use shared RELRO section for %s", libFilePath);
        } else {
            if (DEBUG) Log.i(TAG, "Using shared RELRO section for %s", libFilePath);
        }
    }

    /**
     * Native method used to load a library.
     *
     * @param library Platform specific library name (e.g. libfoo.so)
     * @param loadAddress Explicit load address, or 0 for randomized one.
     * @param libInfo The mLoadAddress and mLoadSize fields
     * of this LibInfo instance will be set on success.
     * @return true for success, false otherwise.
     */
    private static native boolean nativeLoadLibrary(
            String library, long loadAddress, @NonNull LibInfo libInfo);

    /**
     * Native method used to add a zip archive or APK to the search path
     * for native libraries. Allows loading directly from it.
     *
     * @param zipFilePath Path of the zip file containing the libraries.
     * @return true for success, false otherwise.
     */
    private static native boolean nativeAddZipArchivePath(String zipFilePath);

    /**
     * Native method used to create a shared RELRO section.
     * If the library was already loaded at the same address using
     * nativeLoadLibrary(), this creates the RELRO for it. Otherwise,
     * this loads a new temporary library at the specified address,
     * creates and extracts the RELRO section from it, then unloads it.
     *
     * @param library Library name.
     * @param loadAddress load address, which can be different from the one
     * used to load the library in the current process!
     * @param libInfo libInfo instance. On success, the mRelroStart, mRelroSize
     * and mRelroFd will be set.
     * @return true on success, false otherwise.
     */
    private static native boolean nativeCreateSharedRelro(
            String library, long loadAddress, LibInfo libInfo);

    /**
     * Native method used to use a shared RELRO section.
     *
     * @param library Library name.
     * @param libInfo A LibInfo instance containing valid RELRO information
     * @return true on success.
     */
    private static native boolean nativeUseSharedRelro(String library, LibInfo libInfo);
}
