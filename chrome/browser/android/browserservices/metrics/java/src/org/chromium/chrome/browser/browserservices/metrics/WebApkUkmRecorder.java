// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.metrics;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.webapps.WebApkDistributor;

/**
 * A class to record User Keyed Metrics relevant to WebAPKs. This will allow us to concentrate on
 * the use cases for the most used WebAPKs.
 */
@JNINamespace("webapk")
@NullMarked
public class WebApkUkmRecorder {
    /**
     * Records the duration, in exponentially-bucketed milliseconds, of a WebAPK session (from
     * launch/foreground to background).
     */
    public static void recordWebApkSessionDuration(
            @Nullable String manifestId,
            @WebApkDistributor int distributor,
            int versionCode,
            long duration) {
        WebApkUkmRecorderJni.get()
                .recordSessionDuration(manifestId, distributor, versionCode, duration);
    }

    /*
     * Records that WebAPK was launched and the reason for the launch.
     */
    public static void recordWebApkLaunch(
            @Nullable String manifestId,
            @WebApkDistributor int distributor,
            int versionCode,
            int source) {
        WebApkUkmRecorderJni.get().recordVisit(manifestId, distributor, versionCode, source);
    }

    /**
     * Records how long the WebAPK was installed and how many times the WebAPK has been launched
     * since the last time that the user clearer Chrome's storage.
     */
    public static void recordWebApkUninstall(
            @Nullable String manifestId,
            @WebApkDistributor int distributor,
            int versionCode,
            int launchCount,
            long installedDurationMs) {
        WebApkUkmRecorderJni.get()
                .recordUninstall(
                        manifestId, distributor, versionCode, launchCount, installedDurationMs);
    }

    @NativeMethods
    interface Natives {
        void recordSessionDuration(
                @Nullable String manifestId, int distributor, int versionCode, long duration);

        void recordVisit(@Nullable String manifestId, int distributor, int versionCode, int source);

        void recordUninstall(
                @Nullable String manifestId,
                int distributor,
                int versionCode,
                int launchCount,
                long installedDurationMs);
    }
}
