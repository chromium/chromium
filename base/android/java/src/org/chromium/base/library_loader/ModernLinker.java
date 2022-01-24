// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.library_loader;

import android.os.SystemClock;

import androidx.annotation.VisibleForTesting;

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

    private static final String DETAILED_LOAD_TIME_HISTOGRAM_PREFIX =
            "ChromiumAndroidLinker.ModernLinkerDetailedLoadTime.";

    ModernLinker() {}

    @Override
    protected boolean keepMemoryReservationUntilLoad() {
        return true;
    }

    @Override
    @GuardedBy("mLock")
    protected void loadLibraryImplLocked(String library, @RelroSharingMode int relroMode) {
        // Only loading monochrome is supported.
        if (!"monochrome".equals(library) || DEBUG) {
            Log.i(TAG, "loadLibraryImplLocked: %s, relroMode=%d", library, relroMode);
        }
        assert mState == State.INITIALIZED; // Only one successful call.

        // Load or declare fallback to System.loadLibrary.
        long beforeLoadMs = SystemClock.uptimeMillis();
        String libFilePath = System.mapLibraryName(library);
        boolean performedModernLoad = true;
        if (relroMode == RelroSharingMode.NO_SHARING) {
            // System.loadLibrary() below implements the fallback.
            performedModernLoad = false;
            mState = State.DONE;
        } else if (relroMode == RelroSharingMode.PRODUCE) {
            loadAndProduceSharedRelro(libFilePath); // Throws on a failed load.
            // Next state is still to "provide relro", even if there is none, to indicate that
            // consuming RELRO is not expected with this Linker instance.
            mState = State.DONE_PROVIDE_RELRO;
        } else {
            assert relroMode == RelroSharingMode.CONSUME;
            loadWithoutProducingRelro(libFilePath); // Does not throw.
            // Done loading the library, but using an externally provided RELRO may happen later.
            mState = State.DONE;
        }
        if (performedModernLoad) {
            recordDetailedLoadTimeSince(
                    beforeLoadMs, relroMode == RelroSharingMode.PRODUCE ? "Produce" : "Consume");
        }

        // Load the library a second time, in order to keep using lazy JNI registration. When
        // loading the library with the Chromium linker, ART doesn't know about our library, so
        // cannot resolve JNI methods lazily. Loading the library a second time makes sure it
        // knows about us.
        //
        // This is not wasteful though, as libraries are reference-counted, and as a consequence the
        // library is not really loaded a second time, and we keep relocation sharing.
        long beforeSystemLoadMs = SystemClock.uptimeMillis();
        try {
            System.loadLibrary(library);
        } catch (UnsatisfiedLinkError e) {
            resetAndThrow("Failed at System.loadLibrary()");
        }
        recordDetailedLoadTimeSince(
                beforeSystemLoadMs, performedModernLoad ? "Second" : "NoSharing");
    }

    private void recordDetailedLoadTimeSince(long sinceMs, String suffix) {
        RecordHistogram.recordTimesHistogram(
                DETAILED_LOAD_TIME_HISTOGRAM_PREFIX + suffix, SystemClock.uptimeMillis() - sinceMs);
    }

    // Loads the library via ModernLinker for later consumption of the RELRO region, throws on
    // failure to allow a safe retry.
    @GuardedBy("mLock")
    private void loadWithoutProducingRelro(String libFilePath) {
        assert mRemoteLibInfo == null || libFilePath.equals(mRemoteLibInfo.mLibFilePath);
        if (!getModernLinkerJni().loadLibrary(
                    libFilePath, mLocalLibInfo, false /* spawnRelroRegion */)) {
            resetAndThrow(String.format("Unable to load library: %s", libFilePath));
        }
        assert mLocalLibInfo.mRelroFd == -1;
    }

    // Loads the library via ModernLinker. Does not throw on failure because in both cases
    // System.loadLibrary() is useful. Records a histogram to count failures.
    @GuardedBy("mLock")
    private void loadAndProduceSharedRelro(String libFilePath) {
        mLocalLibInfo.mLibFilePath = libFilePath;
        if (getModernLinkerJni().loadLibrary(
                    libFilePath, mLocalLibInfo, true /* spawnRelroRegion */)) {
            if (DEBUG) {
                Log.i(TAG, "Successfully spawned RELRO: mLoadAddress=0x%x, mLoadSize=%d",
                        mLocalLibInfo.mLoadAddress, mLocalLibInfo.mLoadSize);
            }
        } else {
            Log.e(TAG, "Unable to load with ModernLinker, using the system linker instead");
            // System.loadLibrary() below implements the fallback.
            mLocalLibInfo.mRelroFd = -1;
        }
        RecordHistogram.recordBooleanHistogram(
                "ChromiumAndroidLinker.RelroProvidedSuccessfully", mLocalLibInfo.mRelroFd != -1);
    }

    @Override
    @GuardedBy("mLock")
    protected void atomicReplaceRelroLocked(boolean relroAvailableImmediately) {
        assert mRemoteLibInfo != null;
        assert mState == State.DONE;
        if (mRemoteLibInfo.mRelroFd == -1) return;
        if (DEBUG) {
            Log.i(TAG, "Received mRemoteLibInfo: mLoadAddress=0x%x, mLoadSize=%d",
                    mRemoteLibInfo.mLoadAddress, mRemoteLibInfo.mLoadSize);
        }
        getModernLinkerJni().useRelros(mRemoteLibInfo);
        mRemoteLibInfo.close();
        if (DEBUG) Log.i(TAG, "Immediate RELRO availability: %b", relroAvailableImmediately);
        RecordHistogram.recordBooleanHistogram(
                "ChromiumAndroidLinker.RelroAvailableImmediately", relroAvailableImmediately);
        int status = getModernLinkerJni().getRelroSharingResult();
        assert status != RelroSharingStatus.NOT_ATTEMPTED;
        RecordHistogram.recordEnumeratedHistogram(
                "ChromiumAndroidLinker.RelroSharingStatus2", status, RelroSharingStatus.COUNT);
    }

    @GuardedBy("mLock")
    private void resetAndThrow(String message) {
        mState = State.INITIALIZED;
        Log.e(TAG, message);
        throw new UnsatisfiedLinkError(message);
    }

    // Intentionally omitting @NativeMethods because generation of the stubs it requires (as
    // GEN_JNI.java) is disabled by the @JniIgnoreNatives.
    interface Natives {
        boolean loadLibrary(String libFilePath, LibInfo libInfo, boolean spawnRelroRegion);
        boolean useRelros(LibInfo libInfo);
        int getRelroSharingResult();
    }

    private static ModernLinker.Natives sNativesInstance;

    static void setModernLinkerNativesForTesting(Natives instance) {
        sNativesInstance = instance;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static ModernLinker.Natives getModernLinkerJni() {
        if (sNativesInstance != null) return sNativesInstance;
        return new ModernLinkerJni(); // R8 optimizes away all construction except the initial one.
    }
}
