// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics.util;

/**
 * Utilities for Android UKM tests. Not to be used outside of testing.
 */
public class UkmUtilsForTest {
    /**
     * True if the UKM Service is enabled.
     */
    public static boolean isEnabled() {
        return nativeIsEnabled();
    }

    /**
     * True if the input |sourceId| exists within the current UKM recording.
     */
    public static boolean hasSourceWithId(long sourceId) {
        return nativeHasSourceWithId(sourceId);
    }

    /**
     * Record a single Source with the given |sourceId| with a dummy URL.
     */
    public static void recordSourceWithId(long sourceId) {
        nativeRecordSourceWithId(sourceId);
    }

    /**
     * Get the UKM clientId.
     */
    public static long getClientId() {
        return nativeGetClientId();
    }

    private static native boolean nativeIsEnabled();
    private static native boolean nativeHasSourceWithId(long sourceId);
    private static native void nativeRecordSourceWithId(long sourceId);
    private static native long nativeGetClientId();
}
