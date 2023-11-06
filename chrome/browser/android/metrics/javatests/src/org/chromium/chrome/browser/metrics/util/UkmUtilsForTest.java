// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics.util;

import org.jni_zero.NativeMethods;

/** Utilities for Android UKM tests. Not to be used outside of testing. */
public class UkmUtilsForTest {
    /** True if the UKM Service is enabled. */
    public static boolean isEnabled() {
        return UkmUtilsForTestJni.get().isEnabled();
    }

    /** True if the input |sourceId| exists within the current UKM recording. */
    public static boolean hasSourceWithId(long sourceId) {
        return UkmUtilsForTestJni.get().hasSourceWithId(sourceId);
    }

    /** Record a single Source with the given |sourceId| with a dummy URL. */
    public static void recordSourceWithId(long sourceId) {
        UkmUtilsForTestJni.get().recordSourceWithId(sourceId);
    }

    /** Get the UKM clientId. */
    public static long getClientId() {
        return UkmUtilsForTestJni.get().getClientId();
    }

    @NativeMethods
    interface Natives {
        boolean isEnabled();

        boolean hasSourceWithId(long sourceId);

        void recordSourceWithId(long sourceId);

        long getClientId();
    }
}
