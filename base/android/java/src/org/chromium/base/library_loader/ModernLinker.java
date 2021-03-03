// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.library_loader;

import org.chromium.base.Log;
import org.chromium.base.annotations.JniIgnoreNatives;
import org.chromium.base.metrics.RecordHistogram;

import javax.annotation.concurrent.GuardedBy;

/**
 * Provides a concrete implementation of the Chromium Linker.
 *
 * This Linker implementation uses the Android M and later system linker to map Chrome and call
 * |JNI_OnLoad()|.
 *
 * For more on the operations performed by the Linker, see {@link Linker}.
 */
@JniIgnoreNatives
class ModernLinker extends Linker {
    // Log tag for this class.
    private static final String TAG = "ModernLinker";

    ModernLinker() {}

    @Override
    @GuardedBy("mLock")
    void loadLibraryImplLocked(String library, boolean isFixedAddressPermitted) {
        // We expect to load monochrome, if it's not the case, log.
        if (!"monochrome".equals(library) || DEBUG) {
            Log.i(TAG, "loadLibraryImpl: %s, %b", library, isFixedAddressPermitted);
        }
        assert mState == State.INITIALIZED; // Only one successful call.

        String libFilePath = System.mapLibraryName(library);
        boolean loadNoRelro = !isFixedAddressPermitted;
        boolean provideRelro = isFixedAddressPermitted && mRelroProducer;
        long loadAddress = isFixedAddressPermitted ? mBaseLoadAddress : 0;

        if (loadNoRelro) {
            // System.loadLibrary() below implements the fallback.
            mState = State.DONE;
        } else if (provideRelro) {
            // Create the shared RELRO, and store it.
            LibInfo libInfo = new LibInfo();
            libInfo.mLibFilePath = libFilePath;
            if (nativeLoadLibrary(libFilePath, loadAddress, libInfo, true /* spawnRelroRegion */)) {
                Log.d(TAG, "Successfully spawned RELRO: mLoadAddress=0x%x, mLoadSize=%d",
                        libInfo.mLoadAddress, libInfo.mLoadSize);
            } else {
                Log.e(TAG, "Unable to load with ModernLinker, using the system linker instead");
                // System.loadLibrary() below implements the fallback.
                libInfo.mRelroFd = -1;
            }
            mLibInfo = libInfo;
            RecordHistogram.recordBooleanHistogram(
                    "ChromiumAndroidLinker.RelroProvidedSuccessfully", libInfo.mRelroFd != -1);

            // Next state is still to "provide relro", even if there is none, to indicate that
            // consuming RELRO is not expected with this Linker instance.
            mState = State.DONE_PROVIDE_RELRO;
        } else {
            // Running in a child process, also with a fixed load address that is suitable for
            // shared RELRO.
            //
            // Two LibInfo objects are used: |mLibInfo| that brings the RELRO FD, and a temporary
            // LibInfo to load the library. Before replacing the library's RELRO with the one from
            // |mLibInfo|, the two objects are compared to make sure the memory ranges and the
            // contents match.
            LibInfo libInfoForLoad = new LibInfo();
            assert libFilePath.equals(mLibInfo.mLibFilePath);
            if (!nativeLoadLibrary(
                        libFilePath, loadAddress, libInfoForLoad, false /* spawnRelroRegion */)) {
                resetAndThrow(String.format("Unable to load library: %s", libFilePath));
            }
            assert libInfoForLoad.mRelroFd == -1;

            // Done loading the library, but using an externally provided RELRO may happen later.
            mState = State.DONE;
        }

        // Load the library a second time, in order to keep using lazy JNI registration. When
        // loading the library with the Chromium linker, ART doesn't know about our library, so
        // cannot resolve JNI methods lazily. Loading the library a second time makes sure it
        // knows about us.
        //
        // This is not wasteful though, as libraries are reference-counted, and as a consequence the
        // library is not really loaded a second time, and we keep relocation sharing.
        try {
            System.loadLibrary(library);
        } catch (UnsatisfiedLinkError e) {
            if (loadNoRelro || provideRelro) resetAndThrow("Cannot load without relro sharing");
            resetAndThrow("Unable to load the library a second time with the system linker");
        }
    }

    @Override
    @GuardedBy("mLock")
    protected void atomicReplaceRelroLocked(boolean relroAvailableImmediately) {
        assert mLibInfo != null;
        assert mState == State.DONE;
        if (mLibInfo.mRelroFd == -1) return;
        Log.d(TAG, "Received mLibInfo: mLoadAddress=0x%x, mLoadSize=%d", mLibInfo.mLoadAddress,
                mLibInfo.mLoadSize);
        nativeUseRelros(mLibInfo);
        mLibInfo.close();
        Log.d(TAG, "Immediate RELRO availability: %b", relroAvailableImmediately);
        RecordHistogram.recordBooleanHistogram(
                "ChromiumAndroidLinker.RelroAvailableImmediately", relroAvailableImmediately);
        int status = nativeGetRelroSharingResult();
        assert status != RelroSharingStatus.NOT_ATTEMPTED;
        RecordHistogram.recordEnumeratedHistogram(
                "ChromiumAndroidLinker.RelroSharingStatus", status, RelroSharingStatus.COUNT);
    }

    @GuardedBy("mLock")
    private void resetAndThrow(String message) {
        mState = State.INITIALIZED;
        Log.e(TAG, message);
        throw new UnsatisfiedLinkError(message);
    }

    private static native boolean nativeLoadLibrary(
            String dlopenExtPath, long loadAddress, LibInfo libInfo, boolean spawnRelroRegion);
    private static native boolean nativeUseRelros(LibInfo libInfo);
    private static native int nativeGetRelroSharingResult();
}
