// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.library_loader;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.TimeUtils.UptimeMillisTimer;
import org.chromium.base.annotations.JniIgnoreNatives;
import org.chromium.base.metrics.RecordHistogram;

import java.io.BufferedReader;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStreamReader;

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

    private static final String DETAILED_LOAD_TIME_HISTOGRAM_PREFIX_BLKIO_CGROUP =
            "ChromiumAndroidLinker.ModernLinkerDetailedLoadTimeByBlkioCgroup.";

    private static final String SUFFIX_UNKNOWN = "Unknown";

    private static final String SELF_CGROUP_FILE_NAME = "/proc/self/cgroup";

    ModernLinker() {}

    @Override
    protected boolean keepMemoryReservationUntilLoad() {
        return true;
    }

    private static String extractBlkioCgroupFromLine(String line) {
        // The contents of /proc/self/cgroup for a background app looks like this:
        // 5:schedtune:/background
        // 4:memory:/
        // 3:cpuset:/background
        // 2:cpu:/system
        // 1:blkio:/background
        // 0::/uid_10179/pid_11869
        //
        // For a foreground app the relevant line looks like this:
        // 1:blkio:/
        int blkioStartsAt = line.indexOf(":blkio:");
        if (blkioStartsAt == -1) return "";
        return line.substring(blkioStartsAt + 7);
    }

    private String readBackgroundStateFromCgroups() {
        String groupName = null;
        try (BufferedReader reader = new BufferedReader(
                     new InputStreamReader(new FileInputStream(SELF_CGROUP_FILE_NAME)));) {
            String line;
            while ((line = reader.readLine()) != null) {
                groupName = extractBlkioCgroupFromLine(line);
                if (!groupName.equals("")) break;
            }
            if (groupName == null || groupName.equals("")) return SUFFIX_UNKNOWN;
        } catch (IOException e) {
            Log.e(TAG, "IOException while reading %s", SELF_CGROUP_FILE_NAME);
            return SUFFIX_UNKNOWN;
        }
        if (groupName.equals("/")) {
            return "Foreground";
        }
        if (groupName.equals("/background")) {
            return "Background";
        }
        Log.e(TAG, "blkio cgroup with unexpected name: '%s'", groupName);
        return SUFFIX_UNKNOWN;
    }

    @Override
    @GuardedBy("mLock")
    protected void loadLibraryImplLocked(String library, @RelroSharingMode int relroMode) {
        // Only loading monochrome is supported.
        if (!"monochrome".equals(library) || DEBUG) {
            Log.i(TAG, "loadLibraryImplLocked: %s, relroMode=%d", library, relroMode);
        }
        assert mState == State.INITIALIZED; // Only one successful call.

        // Determine whether library loading starts in a foreground or a background cgroup for the
        // 'blkio' controller.
        String backgroundStateBeforeLoad = readBackgroundStateFromCgroups();

        // Load or declare fallback to System.loadLibrary.
        UptimeMillisTimer timer = new UptimeMillisTimer();
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

        // The app can change the bg/fg state while loading the native library, but mostly only
        // once. To reduce the likelihood of a foreground sample to be affected by partially
        // backgrounded state, move the mixed samples to a separate category. The data collected may
        // help proving this hypothesis: "The ModernLinker is not a lot slower than the system
        // linker when running in foreground".
        String backgroundStateAfterLoad = readBackgroundStateFromCgroups();
        if (!backgroundStateBeforeLoad.equals(backgroundStateAfterLoad)) {
            if (backgroundStateBeforeLoad.equals(SUFFIX_UNKNOWN)
                    || backgroundStateAfterLoad.equals(SUFFIX_UNKNOWN)) {
                backgroundStateBeforeLoad = SUFFIX_UNKNOWN;
            } else {
                backgroundStateBeforeLoad = "Mixed";
            }
        }

        if (performedModernLoad) {
            recordDetailedLoadTimeSince(timer,
                    relroMode == RelroSharingMode.PRODUCE ? "Produce" : "Consume",
                    backgroundStateBeforeLoad);
        }

        // Load the library a second time, in order to keep using lazy JNI registration. When
        // loading the library with the Chromium linker, ART doesn't know about our library, so
        // cannot resolve JNI methods lazily. Loading the library a second time makes sure it
        // knows about us.
        //
        // This is not wasteful though, as libraries are reference-counted, and as a consequence the
        // library is not really loaded a second time, and we keep relocation sharing.
        timer = new UptimeMillisTimer();
        try {
            System.loadLibrary(library);
        } catch (UnsatisfiedLinkError e) {
            resetAndThrow("Failed at System.loadLibrary()");
        }
        recordDetailedLoadTimeSince(
                timer, performedModernLoad ? "Second" : "NoSharing", backgroundStateBeforeLoad);
    }

    private void recordDetailedLoadTimeSince(
            UptimeMillisTimer timer, String suffix, String backgroundStateSuffix) {
        long durationMs = timer.getElapsedMillis();
        RecordHistogram.recordTimesHistogram(
                DETAILED_LOAD_TIME_HISTOGRAM_PREFIX + suffix, durationMs);
        RecordHistogram.recordTimesHistogram(DETAILED_LOAD_TIME_HISTOGRAM_PREFIX_BLKIO_CGROUP
                        + suffix + "." + backgroundStateSuffix,
                durationMs);
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
        if (mLocalLibInfo == null) return;
        getModernLinkerJni().useRelros(mLocalLibInfo.mLoadAddress, mRemoteLibInfo);
        // *Not* closing the RELRO FD after using it because the FD may need to be transferred to
        // another process after this point.
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

    public static void reportDlopenExtTime(long millis) {
        RecordHistogram.recordTimesHistogram(
                "ChromiumAndroidLinker.ModernLinkerDlopenExtTime", millis);
    }

    public static void reportIteratePhdrTime(long millis) {
        RecordHistogram.recordTimesHistogram(
                "ChromiumAndroidLinker.ModernLinkerIteratePhdrTime", millis);
    }

    // Intentionally omitting @NativeMethods because generation of the stubs it requires (as
    // GEN_JNI.java) is disabled by the @JniIgnoreNatives.
    interface Natives {
        boolean loadLibrary(String libFilePath, LibInfo libInfo, boolean spawnRelroRegion);
        boolean useRelros(long localLoadAddress, LibInfo remoteLibInfo);
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
