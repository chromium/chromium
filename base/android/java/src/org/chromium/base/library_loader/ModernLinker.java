// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.library_loader;

import android.os.Build;

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
    private static final String TAG = "ModernLinker";

    // Whether to use memfd_create(2) for creating RELRO FD on supported systems.
    // TODO(pasko): Remove this compile time constant when memfd reaches Stable (M91).
    private static final boolean ALLOW_MEMFD = true;

    ModernLinker() {}

    @Override
    protected boolean keepMemoryReservationUntilLoad() {
        return true;
    }

    private static boolean useMemfd() {
        return ALLOW_MEMFD && (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R);
    }

    @Override
    @GuardedBy("mLock")
    protected void loadLibraryImplLocked(String library, @RelroSharingMode int relroMode) {
        // Only loading monochrome is supported.
        if (!"monochrome".equals(library) || DEBUG) {
            Log.i(TAG, "loadLibraryImplLocked: %s, relroMode=%d", library, relroMode);
        }
        assert mState == State.INITIALIZED; // Only one successful call.

        String libFilePath = System.mapLibraryName(library);
        if (relroMode == RelroSharingMode.NO_SHARING) {
            // System.loadLibrary() below implements the fallback.
            mState = State.DONE;
        } else if (relroMode == RelroSharingMode.PRODUCE) {
            // Create the shared RELRO, and store it.
            mLocalLibInfo.mLibFilePath = libFilePath;
            if (nativeLoadLibrary(
                        libFilePath, mLocalLibInfo, true /* spawnRelroRegion */, useMemfd())) {
                Log.d(TAG, "Successfully spawned RELRO: mLoadAddress=0x%x, mLoadSize=%d",
                        mLocalLibInfo.mLoadAddress, mLocalLibInfo.mLoadSize);
            } else {
                Log.e(TAG, "Unable to load with ModernLinker, using the system linker instead");
                // System.loadLibrary() below implements the fallback.
                mLocalLibInfo.mRelroFd = -1;
            }
            RecordHistogram.recordBooleanHistogram(
                    "ChromiumAndroidLinker.RelroProvidedSuccessfully",
                    mLocalLibInfo.mRelroFd != -1);

            // Next state is still to "provide relro", even if there is none, to indicate that
            // consuming RELRO is not expected with this Linker instance.
            mState = State.DONE_PROVIDE_RELRO;
        } else {
            assert relroMode == RelroSharingMode.CONSUME;
            assert libFilePath.equals(mRemoteLibInfo.mLibFilePath);
            if (!nativeLoadLibrary(
                        libFilePath, mLocalLibInfo, false /* spawnRelroRegion */, useMemfd())) {
                resetAndThrow(String.format("Unable to load library: %s", libFilePath));
            }
            assert mLocalLibInfo.mRelroFd == -1;

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
            resetAndThrow("Failed at System.loadLibrary()");
        }
    }

    @Override
    @GuardedBy("mLock")
    protected void atomicReplaceRelroLocked(boolean relroAvailableImmediately) {
        assert mRemoteLibInfo != null;
        assert mState == State.DONE;
        if (mRemoteLibInfo.mRelroFd == -1) return;
        Log.d(TAG, "Received mRemoteLibInfo: mLoadAddress=0x%x, mLoadSize=%d",
                mRemoteLibInfo.mLoadAddress, mRemoteLibInfo.mLoadSize);
        nativeUseRelros(mRemoteLibInfo, useMemfd());
        mRemoteLibInfo.close();
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
            String libFilePath, LibInfo libInfo, boolean spawnRelroRegion, boolean useMemfd);
    private static native boolean nativeUseRelros(LibInfo libInfo, boolean useMemfd);
    private static native int nativeGetRelroSharingResult();
}
