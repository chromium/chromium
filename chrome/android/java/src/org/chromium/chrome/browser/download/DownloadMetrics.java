// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import androidx.annotation.Nullable;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.profile_metrics.BrowserProfileType;

/** Records download related metrics on Android. */
public class DownloadMetrics {
    /**
     * Records download open source.
     * @param source The source where the user opened the download media file.
     * @param mimeType The mime type of the download.
     */
    public static void recordDownloadOpen(@DownloadOpenSource int source, String mimeType) {
        @DownloadFilter.Type int type = DownloadFilter.fromMimeType(mimeType);
        if (type == DownloadFilter.Type.VIDEO) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Android.DownloadManager.OpenSource.Video",
                    source,
                    DownloadOpenSource.MAX_VALUE);
        } else if (type == DownloadFilter.Type.AUDIO) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Android.DownloadManager.OpenSource.Audio",
                    source,
                    DownloadOpenSource.MAX_VALUE);
        } else {
            RecordHistogram.recordEnumeratedHistogram(
                    "Android.DownloadManager.OpenSource.Other",
                    source,
                    DownloadOpenSource.MAX_VALUE);
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

        Profile profile = tab.getProfile();
        @BrowserProfileType int type = Profile.getBrowserProfileTypeFromProfile(profile);
        RecordHistogram.recordEnumeratedHistogram(
                "Download.OpenDownloads.PerProfileType", type, BrowserProfileType.MAX_VALUE + 1);
        if (source == DownloadOpenSource.MENU) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Download.OpenDownloadsFromMenu.PerProfileType",
                    type,
                    BrowserProfileType.MAX_VALUE + 1);
        }
    }
}
