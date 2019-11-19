// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.downloads;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.download.DownloadInfo;
import org.chromium.chrome.browser.download.DownloadManagerService;
import org.chromium.chrome.browser.download.DownloadNotifier;
import org.chromium.chrome.browser.download.DownloadSharedPreferenceEntry;
import org.chromium.chrome.browser.download.DownloadSharedPreferenceHelper;
import org.chromium.chrome.browser.download.items.OfflineContentAggregatorFactory;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.browser.offlinepages.OfflinePageOrigin;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.FailState;
import org.chromium.components.offline_items_collection.LegacyHelpers;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.PendingState;
import org.chromium.ui.widget.Toast;

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

        DownloadInfo downloadInfo = new DownloadInfo.Builder()
                                            .setIsOfflinePage(true)
                                            .setDownloadGuid(guid)
                                            .setFileName(displayName)
                                            .setIsResumable(false)
                                            .setIsOffTheRecord(false)
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

        DownloadInfo downloadInfo = new DownloadInfo.Builder()
                .setIsOfflinePage(true).setDownloadGuid(guid).setFileName(displayName).build();

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

        DownloadInfo downloadInfo = new DownloadInfo.Builder()
                                            .setIsOfflinePage(true)
                                            .setDownloadGuid(guid)
                                            .setFileName(displayName)
                                            .setFilePath(url)
                                            .setBytesReceived(bytesReceived)
                                            .setIsOffTheRecord(false)
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

        DownloadInfo downloadInfo = new DownloadInfo.Builder()
                .setIsOfflinePage(true).setDownloadGuid(guid).setFileName(displayName).build();

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

        DownloadInfo downloadInfo = new DownloadInfo.Builder()
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

    /**
     * Aborts the notification.
     *
     * @param guid GUID of a request to download a page related to the notification.
     */
    private static void suppressNotification(String guid) {
        DownloadNotifier notifier = getDownloadNotifier();
        if (notifier == null) return;

        ContentId id = LegacyHelpers.buildLegacyContentId(true, guid);

        DownloadSharedPreferenceEntry entry =
                DownloadSharedPreferenceHelper.getInstance().getDownloadSharedPreferenceEntry(id);

        if (entry == null) return;

        DownloadInfo downloadInfo = new DownloadInfo.Builder().setContentId(id).build();

        notifier.removeDownloadNotification(entry.notificationId, downloadInfo);
    }

    /**
     * Returns whether we should suppress download complete notification based
     * on the origin app of the download.
     * @param originString the qualified string form of an OfflinePageOrigin
     */
    private static boolean shouldSuppressCompletedNotification(String originString) {
        OfflinePageOrigin origin = new OfflinePageOrigin(originString);
        return AppHooks.get().getOfflinePagesSuppressNotificationPackages().contains(
                origin.getAppName());
    }

    /**
     * Returns whether the notification is suppressed. Suppression is determined
     * based on the origin app of the download.
     *
     * @param originString the qualified string form of an OfflinePageOrigin
     * @param guid GUID of a request to download a page related to the notification.
     */
    @CalledByNative
    private static boolean maybeSuppressNotification(String originString, String guid) {
        if (shouldSuppressCompletedNotification(originString)) {
            suppressNotification(guid);
            return true;
        }
        return false;
    }

    /**
     * Shows a "Downloading ..." toast for the requested items already scheduled for download.
     */
    @CalledByNative
    public static void showDownloadingToast() {
        if (FeatureUtilities.isDownloadProgressInfoBarEnabled()) {
            intializeOfflineItemsCollection();
            DownloadManagerService.getDownloadManagerService()
                    .getInfoBarController(false)
                    .onDownloadStarted();
        } else {
            Toast.makeText(ContextUtils.getApplicationContext(), R.string.download_started,
                         Toast.LENGTH_SHORT)
                    .show();
        }
    }

    /**
     * TODO(shaktisahu): Remove this function when offline pages backend cache loading is fixed.
     */
    private static void intializeOfflineItemsCollection() {
        OfflineContentProvider offlineContentProvider = OfflineContentAggregatorFactory.get();
        offlineContentProvider.getAllItems(offlineItems -> {});
    }

    private static DownloadNotifier getDownloadNotifier() {
        return DownloadManagerService.getDownloadManagerService().getDownloadNotifier();
    }
}
