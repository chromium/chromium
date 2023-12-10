// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.metrics;

/** Holds the {@link CachingUmaRecorder} used by {@link RecordHistogram}. */
public class UmaRecorderHolder {
    private UmaRecorderHolder() {}

    /** The instance held by this class. */
    private static CachingUmaRecorder sRecorder = new CachingUmaRecorder();

    /** Set up native UMA Recorder */
    private static boolean sSetUpNativeUmaRecorder = true;

    /** Whether onLibraryLoaded() was called. */
    private static boolean sNativeInitialized;

    /** Returns the held {@link UmaRecorder}. */
    public static UmaRecorder get() {
        return sRecorder;
    }

    /**
     * Set a new {@link UmaRecorder} delegate for the {@link CachingUmaRecorder}.
     * Returns after the cache has been flushed to the new delegate.
     * <p>
     * It should be used in processes that don't or haven't loaded native yet. This should never
     * be called after calling {@link #onLibraryLoaded()}.
     *
     * @param recorder the new UmaRecorder that metrics will be forwarded to.
     */
    public static void setNonNativeDelegate(UmaRecorder recorder) {
        UmaRecorder previous = sRecorder.setDelegate(recorder);
        assert !(previous instanceof NativeUmaRecorder)
                : "A NativeUmaRecorder has already been set";
    }

    /**
     * Whether a native UMA Recorder should be set up.
     * @param setUpNativeUmaRecorder indicates whether a native UMA recorder should be set up.
     * Defaults to true if unset.
     */
    public static void setUpNativeUmaRecorder(boolean setUpNativeUmaRecorder) {
        sSetUpNativeUmaRecorder = setUpNativeUmaRecorder;
    }

    /** Starts forwarding metrics to the native code. Returns after the cache has been flushed. */
    public static void onLibraryLoaded() {
        if (!sSetUpNativeUmaRecorder) return;

        assert !sNativeInitialized;
        sNativeInitialized = true;
        sRecorder.setDelegate(new NativeUmaRecorder());
    }

    /** Reset globals for tests. */
    public static void resetForTesting() {
        // Prevent hitting cache size limits from tests running without ever switching to the native
        // recorder. Also guards against tests that use setNonNativeDelegate() to inject a mock from
        // forgetting to reset it.
        if (!sNativeInitialized) {
            sRecorder = new CachingUmaRecorder();
        }
    }
}
