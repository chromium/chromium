// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;

import java.util.ArrayList;

/**
 * Records download related metrics on Android.
 */
public class DownloadMetrics {
    private static final String TAG = "DownloadMetrics";
    private static final int MAX_VIEW_RETENTION_MINUTES = 30 * 24 * 60;

    /**
     * Records download open source.
     * @param source The source where the user opened the download media file.
     * @param mimeType The mime type of the download.
     */
    public static void recordDownloadOpen(@DownloadOpenSource int source, String mimeType) {
        if (!isNativeLoaded()) {
            Log.w(TAG, "Native is not loaded, dropping download open metrics.");
            return;
        }

        @DownloadFilter.Type
        int type = DownloadFilter.fromMimeType(mimeType);
        if (type == DownloadFilter.Type.VIDEO) {
            RecordHistogram.recordEnumeratedHistogram("Android.DownloadManager.OpenSource.Video",
                    source, DownloadOpenSource.MAX_VALUE);
        } else if (type == DownloadFilter.Type.AUDIO) {
            RecordHistogram.recordEnumeratedHistogram("Android.DownloadManager.OpenSource.Audio",
                    source, DownloadOpenSource.MAX_VALUE);
        } else {
            RecordHistogram.recordEnumeratedHistogram("Android.DownloadManager.OpenSource.Other",
                    source, DownloadOpenSource.MAX_VALUE);
        }
    }

    public static void recordDownloadPageOpen(@DownloadOpenSource int source) {
        if (!isNativeLoaded()) {
            Log.w(TAG, "Native is not loaded, dropping download open metrics.");
            return;
        }
        RecordHistogram.recordEnumeratedHistogram(
                "Android.DownloadPage.OpenSource", source, DownloadOpenSource.MAX_VALUE);
    }

    /**
     * Records how long does the user keep the download file on disk when the user tries to open
     * the file.
     * @param mimeType The mime type of the download.
     * @param startTime The start time of the download.
     */
    public static void recordDownloadViewRetentionTime(String mimeType, long startTime) {
        if (!isNativeLoaded()) {
            Log.w(TAG, "Native is not loaded, dropping download view retention metrics.");
            return;
        }

        @DownloadFilter.Type
        int type = DownloadFilter.fromMimeType(mimeType);
        int viewRetentionTimeMinutes = (int) ((System.currentTimeMillis() - startTime) / 60000);

        if (type == DownloadFilter.Type.VIDEO) {
            RecordHistogram.recordCustomCountHistogram(
                    "Android.DownloadManager.ViewRetentionTime.Video", viewRetentionTimeMinutes, 1,
                    MAX_VIEW_RETENTION_MINUTES, 50);
        } else if (type == DownloadFilter.Type.AUDIO) {
            RecordHistogram.recordCustomCountHistogram(
                    "Android.DownloadManager.ViewRetentionTime.Audio", viewRetentionTimeMinutes, 1,
                    MAX_VIEW_RETENTION_MINUTES, 50);
        }
    }

    /**
     * Records download directory type when a download is completed.
     * @param filePath The absolute file path of the download.
     */
    public static void recordDownloadDirectoryType(String filePath) {
        if (filePath == null || filePath.isEmpty()) return;

        DownloadDirectoryProvider.getInstance().getAllDirectoriesOptions(
                (ArrayList<DirectoryOption> dirs) -> {
                    for (DirectoryOption dir : dirs) {
                        if (filePath.contains(dir.location)) {
                            RecordHistogram.recordEnumeratedHistogram(
                                    "MobileDownload.Location.Download.DirectoryType", dir.type,
                                    DirectoryOption.DownloadLocationDirectoryType.NUM_ENTRIES);
                            return;
                        }
                    }
                });
    }

    private static boolean isNativeLoaded() {
        return ChromeBrowserInitializer.getInstance(ContextUtils.getApplicationContext())
                .hasNativeInitializationCompleted();
    }
}
