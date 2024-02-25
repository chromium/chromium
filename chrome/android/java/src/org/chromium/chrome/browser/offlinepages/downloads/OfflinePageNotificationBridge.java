// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.downloads;

import org.jni_zero.CalledByNative;

import org.chromium.chrome.browser.download.DownloadInfo;
import org.chromium.chrome.browser.download.DownloadManagerService;
import org.chromium.chrome.browser.download.DownloadNotifier;
import org.chromium.chrome.browser.download.items.OfflineContentAggregatorFactory;
import org.chromium.components.offline_items_collection.FailState;
import org.chromium.components.offline_items_collection.LegacyHelpers;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.PendingState;

/**
 * Class for dispatching offline page/request related notifications to the
 * {org.chromium.chrome.browser.download.DownloadNotifier}.
 */
public class OfflinePageNotificationBridge {
    /**
     * Update download notification to success.
     *
     * @param guid             GUID of a request to download a page related to the notification.
     * @param url              URL of the page to download.
     * @param displayName      Name to be displayed on notification.
     * @param networkBytesUsed The total number of bytes downloaded for this item.
     */
    @CalledByNative
    public static void notifyDownloadSuccessful(
            String guid, String url, String displayName, long networkBytesUsed) {
        DownloadNotifier notifier = getDownloadNotifier();
        if (notifier == null) return;

        DownloadInfo downloadInfo =
                new DownloadInfo.Builder()
                        .setIsOfflinePage(true)
                        .setDownloadGuid(guid)
                        .setFileName(displayName)
                        .setIsResumable(false)
                        .setOTRProfileId(null)
                        .setBytesTotalSize(networkBytesUsed)
                        .build();

        notifier.notifyDownloadSuccessful(downloadInfo, -1, false, true);
    }

    /**
     * Update download notification to failure.
     *
     * @param guid        GUID of a request to download a page related to the notification.
     * @param url         URL of the page to download.
     * @param displayName Name to be displayed on notification.
     * @param failState   The reason the download failed.
     */
    @CalledByNative
    public static void notifyDownloadFailed(
            String guid, String url, String displayName, @FailState int failState) {
        DownloadNotifier notifier = getDownloadNotifier();
        if (notifier == null) return;

        DownloadInfo downloadInfo =
                new DownloadInfo.Builder()
                        .setIsOfflinePage(true)
                        .setDownloadGuid(guid)
                        .setFileName(displayName)
                        .build();

        notifier.notifyDownloadFailed(downloadInfo, failState);
    }

    /**
     * Called by offline page backend to notify the user of download progress.
     *
     * @param guid        GUID of a request to download a page related to the notification.
     * @param url         URL of the page to download.
     * @param startTime   Time of the request.
     * @param displayName Name to be displayed on notification.
     */
    @CalledByNative
    public static void notifyDownloadProgress(
            String guid, String url, long startTime, long bytesReceived, String displayName) {
        DownloadNotifier notifier = getDownloadNotifier();
        if (notifier == null) return;

        DownloadInfo downloadInfo =
                new DownloadInfo.Builder()
                        .setIsOfflinePage(true)
                        .setDownloadGuid(guid)
                        .setFileName(displayName)
                        .setFilePath(url)
                        .setBytesReceived(bytesReceived)
                        .setOTRProfileId(null)
                        .setIsResumable(true)
                        .setTimeRemainingInMillis(0)
                        .build();

        notifier.notifyDownloadProgress(downloadInfo, startTime, false);
    }

    /**
     * Update download notification to paused.
     *
     * @param guid        GUID of a request to download a page related to the notification.
     * @param displayName Name to be displayed on notification.
     */
    @CalledByNative
    public static void notifyDownloadPaused(String guid, String displayName) {
        DownloadNotifier notifier = getDownloadNotifier();
        if (notifier == null) return;

        DownloadInfo downloadInfo =
                new DownloadInfo.Builder()
                        .setIsOfflinePage(true)
                        .setDownloadGuid(guid)
                        .setFileName(displayName)
                        .build();

        notifier.notifyDownloadPaused(downloadInfo);
    }

    /**
     * Update download notification to interrupted.
     *
     * @param guid        GUID of a request to download a page related to the notification.
     * @param displayName Name to be displayed on notification.
     */
    @CalledByNative
    public static void notifyDownloadInterrupted(
            String guid, String displayName, @PendingState int pendingState) {
        DownloadNotifier notifier = getDownloadNotifier();
        if (notifier == null) return;

        DownloadInfo downloadInfo =
                new DownloadInfo.Builder()
                        .setIsOfflinePage(true)
                        .setDownloadGuid(guid)
                        .setFileName(displayName)
                        .setIsResumable(true)
                        .build();

        notifier.notifyDownloadInterrupted(downloadInfo, true, pendingState);
    }

    /**
     * Update download notification to canceled.
     *
     * @param guid GUID of a request to download a page related to the notification.
     */
    @CalledByNative
    public static void notifyDownloadCanceled(String guid) {
        DownloadNotifier notifier = getDownloadNotifier();
        if (notifier == null) return;

        notifier.notifyDownloadCanceled(LegacyHelpers.buildLegacyContentId(true, guid));
    }

    /** Shows a "Downloading ..." toast for the requested items already scheduled for download. */
    @CalledByNative
    public static void showDownloadingToast() {
        intializeOfflineItemsCollection();
        DownloadManagerService.getDownloadManagerService()
                .getMessageUiController(/* otrProfileID= */ null)
                .onDownloadStarted();
    }

    /** TODO(shaktisahu): Remove this function when offline pages backend cache loading is fixed. */
    private static void intializeOfflineItemsCollection() {
        OfflineContentProvider offlineContentProvider = OfflineContentAggregatorFactory.get();
        offlineContentProvider.getAllItems(offlineItems -> {});
    }

    private static DownloadNotifier getDownloadNotifier() {
        return DownloadManagerService.getDownloadManagerService().getDownloadNotifier();
    }
}
