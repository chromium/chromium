// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.profile_metrics.BrowserProfileType;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Records download related metrics on Android. */
@NullMarked
public class DownloadMetrics {
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        OpenWithExternalAppsSource.OPEN_FILE,
        OpenWithExternalAppsSource.DOWNLOAD_PROGRESS_MESSAGE,
        OpenWithExternalAppsSource.APP_MENU,
        OpenWithExternalAppsSource.DOWNLOAD_HOME_MENU,
        OpenWithExternalAppsSource.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface OpenWithExternalAppsSource {
        int OPEN_FILE = 0;
        int DOWNLOAD_PROGRESS_MESSAGE = 1;
        int APP_MENU = 2;
        int DOWNLOAD_HOME_MENU = 3;

        int NUM_ENTRIES = 4;
    }

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        DownloadOpenTarget.CHROME_DEFAULT,
        DownloadOpenTarget.CHROME_FALLBACK,
        DownloadOpenTarget.OTHER_APP_DEFAULT,
        DownloadOpenTarget.OS_CHOOSER,
        DownloadOpenTarget.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface DownloadOpenTarget {
        int CHROME_DEFAULT = 0;
        int CHROME_FALLBACK = 1;
        int OTHER_APP_DEFAULT = 2;
        int OS_CHOOSER = 3;

        int NUM_ENTRIES = 4;
    }

    /**
     * Records download open source.
     *
     * @param source The source where the user opened the download media file.
     * @param mimeType The mime type of the download.
     */
    public static void recordDownloadOpen(
            @DownloadOpenSource int source, @Nullable String mimeType) {
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
                "Download.OpenDownloads.PerProfileType", type, BrowserProfileType.MAX_VALUE);
        if (source == DownloadOpenSource.MENU) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Download.OpenDownloadsFromMenu.PerProfileType",
                    type,
                    BrowserProfileType.MAX_VALUE);
        }
    }

    /**
     * Record the source when downloads are opened with external app.
     *
     * @param openWithExternalAppsSource The source when download is opened with external app.
     */
    public static void recordOpenDownloadWithExternalAppsSource(
            @OpenWithExternalAppsSource int openWithExternalAppsSource) {
        RecordHistogram.recordEnumeratedHistogram(
                "Download.OpenDownloads.OpenWithExternalAppsSource",
                openWithExternalAppsSource,
                OpenWithExternalAppsSource.NUM_ENTRIES);
    }

    /**
     * Records download open target when a download is opened.
     *
     * @param target The target application where the download is opened.
     */
    public static void recordDownloadOpenTarget(@DownloadOpenTarget int target) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.Download.OpenTarget", target, DownloadOpenTarget.NUM_ENTRIES);
    }
}
