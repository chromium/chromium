// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import androidx.annotation.Nullable;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.profile_metrics.BrowserProfileType;

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

    /**
     * Records download home open metrics.
     * @param source The source where the user opened the download media file.
     * @param tab The active tab when opening download manager to reach profile.
     */
    public static void recordDownloadPageOpen(@DownloadOpenSource int source, @Nullable Tab tab) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.DownloadPage.OpenSource", source, DownloadOpenSource.MAX_VALUE);

        // Below there are metrics per profile type, so there should be a tab to get profile.
        if (tab == null) return;

        Profile profile = Profile.fromWebContents(tab.getWebContents());
        if (profile == null) return;

        @BrowserProfileType
        int type = Profile.getBrowserProfileTypeFromProfile(profile);
        RecordHistogram.recordEnumeratedHistogram(
                "Download.OpenDownloads.PerProfileType", type, BrowserProfileType.MAX_VALUE + 1);
        if (source == DownloadOpenSource.MENU) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Download.OpenDownloadsFromMenu.PerProfileType", type,
                    BrowserProfileType.MAX_VALUE + 1);
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
}
