// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.util;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

@JNINamespace("android_webview")
public class TrafficStatsTestUtil {

    public TrafficStatsTestUtil() {}

    /** Returns whether the device supports calling nativeGetTaggedBytes(). */
    public static boolean nativeCanGetTaggedBytes() {
        return TrafficStatsTestUtilJni.get().canGetTaggedBytes();
    }

    /**
     * Query the system to find out how many bytes were received with tag {@code expectedTag} for
     * our UID.
     *
     * @param expectedTag the tag to query for.
     * @return the count of received bytes.
     */
    public static long nativeGetTaggedBytes(int expectedTag) {
        return TrafficStatsTestUtilJni.get().getTaggedBytes(expectedTag);
    }

    @NativeMethods
    interface Natives {
        boolean canGetTaggedBytes();

        long getTaggedBytes(int expectedTag);
    }
}
