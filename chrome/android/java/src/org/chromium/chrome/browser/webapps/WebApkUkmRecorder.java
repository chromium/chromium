// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;
import org.chromium.base.annotations.NativeMethods;

/**
 * A class to record User Keyed Metrics relevant to WebAPKs. This
 * will allow us to concentrate on the use cases for the most used WebAPKs.
 */
public class WebApkUkmRecorder {
    /**
     * Records the duration, in exponentially-bucketed milliseconds, of a WebAPK session (from
     * launch/foreground to background).
     */
    public static void recordWebApkSessionDuration(String manifestUrl,
            @WebApkDistributor int distributor, int versionCode, long duration) {
        WebApkUkmRecorderJni.get().recordSessionDuration(
                manifestUrl, distributor, versionCode, duration);
    }

    /*
     * Records that WebAPK was launched and the reason for the launch.
     */
    public static void recordWebApkLaunch(
            String manifestUrl, @WebApkDistributor int distributor, int versionCode, int source) {
        WebApkUkmRecorderJni.get().recordVisit(manifestUrl, distributor, versionCode, source);
    }

    /**
     * Records how long the WebAPK was installed and how many times the WebAPK has been launched
     * since the last time that the user clearer Chrome's storage.
     */
    public static void recordWebApkUninstall(String manifestUrl, @WebApkDistributor int distributor,
            int versionCode, int launchCount, long installedDurationMs) {
        WebApkUkmRecorderJni.get().recordUninstall(
                manifestUrl, distributor, versionCode, launchCount, installedDurationMs);
    }

    @NativeMethods
    interface Natives {
        void recordSessionDuration(
                String manifestUrl, int distributor, int versionCode, long duration);
        void recordVisit(String manifestUrl, int distributor, int versionCode, int source);
        void recordUninstall(String manifestUrl, int distributor, int versionCode, int launchCount,
                long installedDurationMs);
    }
}
