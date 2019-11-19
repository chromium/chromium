// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static org.chromium.chrome.browser.download.DownloadBroadcastManager.getServiceDelegate;
import static org.chromium.chrome.browser.download.DownloadSnackbarController.INVALID_NOTIFICATION_ID;

import android.app.Notification;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.drawable.shapes.OvalShape;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.browser.notifications.NotificationManagerProxy;
import org.chromium.chrome.browser.notifications.NotificationManagerProxyImpl;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.FailState;
import org.chromium.components.offline_items_collection.LegacyHelpers;
import org.chromium.components.offline_items_collection.OfflineItem.Progress;
import org.chromium.components.offline_items_collection.PendingState;
import org.chromium.content_public.browser.BrowserStartupController;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/**
 * Central director for updates related to downloads and notifications.
 *  - Receive updates about downloads through SystemDownloadNotifier (notifyDownloadPaused, etc).
 *  - Create notifications for downloads using DownloadNotificationFactory.
 *  - Update DownloadForegroundServiceManager about downloads, allowing it to start/stop service.
 */
public class DownloadNotificationService {
    @IntDef({DownloadStatus.IN_PROGRESS, DownloadStatus.PAUSED, DownloadStatus.COMPLETED,
            DownloadStatus.CANCELLED, DownloadStatus.FAILED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface DownloadStatus {
        int IN_PROGRESS = 0;
        int PAUSED = 1;
        int COMPLETED = 2;
        int CANCELLED = 3;
        int FAILED = 4;
    }

    public static final String ACTION_DOWNLOAD_CANCEL =
            "org.chromium.chrome.browser.download.DOWNLOAD_CANCEL";
    public static final String ACTION_DOWNLOAD_PAUSE =
            "org.chromium.chrome.browser.download.DOWNLOAD_PAUSE";
    public static final String ACTION_DOWNLOAD_RESUME =
            "org.chromium.chrome.browser.download.DOWNLOAD_RESUME";
    static final String ACTION_DOWNLOAD_OPEN = "org.chromium.chrome.browser.download.DOWNLOAD_OPEN";

    static final String EXTRA_DOWNLOAD_CONTENTID_ID =
            "org.chromium.chrome.browser.download.DownloadContentId_Id";
    static final String EXTRA_DOWNLOAD_CONTENTID_NAMESPACE =
            "org.chromium.chrome.browser.download.DownloadContentId_Namespace";
    static final String EXTRA_DOWNLOAD_FILE_PATH = "DownloadFilePath";
    static final String EXTRA_IS_SUPPORTED_MIME_TYPE = "IsSupportedMimeType";
    static final String EXTRA_IS_OFF_THE_RECORD =
            "org.chromium.chrome.browser.download.IS_OFF_THE_RECORD";
    // Used to propagate request state information for OfflineItems.StateAtCancel UMA.
    static final String EXTRA_DOWNLOAD_STATE_AT_CANCEL =
            "org.chromium.chrome.browser.download.OfflineItemsStateAtCancel";

    static final String EXTRA_NOTIFICATION_BUNDLE_ICON_ID = "Chrome.NotificationBundleIconIdExtra";
    static final String EXTRA_IS_AUTO_RESUMPTION =
            "org.chromium.chrome.browser.download.IS_AUTO_RESUMPTION";
    /** Notification Id starting value, to avoid conflicts from IDs used in prior versions. */
    private static final int STARTING_NOTIFICATION_ID = 1000000;

    private static final String KEY_NEXT_DOWNLOAD_NOTIFICATION_ID = "NextDownloadNotificationId";

    private static final int MAX_RESUMPTION_ATTEMPT_LEFT = 5;
    private static final String KEY_AUTO_RESUMPTION_ATTEMPT_LEFT = "ResumptionAttemptLeft";

    @VisibleForTesting
    final List<ContentId> mDownloadsInProgress = new ArrayList<ContentId>();

    private NotificationManagerProxy mNotificationManager;
    private Bitmap mDownloadSuccessLargeIcon;
    private DownloadSharedPreferenceHelper mDownloadSharedPreferenceHelper;
    private DownloadForegroundServiceManager mDownloadForegroundServiceManager;

    private static class LazyHolder {
        private static final DownloadNotificationService INSTANCE =
                new DownloadNotificationService();
    }

    /**
     * Creates DownloadNotificationService.
     */
    public static DownloadNotificationService getInstance() {
        return LazyHolder.INSTANCE;
    }

    @VisibleForTesting
    DownloadNotificationService() {
        mNotificationManager =
                new NotificationManagerProxyImpl(ContextUtils.getApplicationContext());
        mDownloadSharedPreferenceHelper = DownloadSharedPreferenceHelper.getInstance();
        mDownloadForegroundServiceManager = new DownloadForegroundServiceManager();
    }

    @VisibleForTesting
    void setDownloadForegroundServiceManager(
            DownloadForegroundServiceManager downloadForegroundServiceManager) {
        mDownloadForegroundServiceManager = downloadForegroundServiceManager;
    }

    /**
     * @return Whether or not there are any current resumable downloads being tracked.  These
     *         tracked downloads may not currently be showing notifications.
     */
    static boolean isTrackingResumableDownloads(Context context) {
        List<DownloadSharedPreferenceEntry> entries =
                DownloadSharedPreferenceHelper.getInstance().getEntries();
        for (DownloadSharedPreferenceEntry entry : entries) {
            if (canResumeDownload(context, entry)) return true;
        }
        return false;
    }

    /**
     * Track in-progress downloads here.
     * @param id The {@link ContentId} of the download that has been started and should be tracked.
     */
    private void startTrackingInProgressDownload(ContentId id) {
        if (!mDownloadsInProgress.contains(id)) mDownloadsInProgress.add(id);
    }

    /**
     * Stop tracking the download represented by {@code id}.
     * @param id                  The {@link ContentId} of the download that has been paused or
     *                            canceled and shouldn't be tracked.
     */
    private void stopTrackingInProgressDownload(ContentId id) {
        mDownloadsInProgress.remove(id);
    }

    /**
     * Adds or updates an in-progress download notification.
     * @param id                      The {@link ContentId} of the download.
     * @param fileName                File name of the download.
     * @param progress                The current download progress.
     * @param bytesReceived           Total number of bytes received.
     * @param timeRemainingInMillis   Remaining download time in milliseconds.
     * @param startTime               Time when download started.
     * @param isOffTheRecord          Whether the download is off the record.
     * @param canDownloadWhileMetered Whether the download can happen in metered network.
     * @param isTransient             Whether or not clicking on the download should launch
     *                                downloads home.
     * @param icon                    A {@link Bitmap} to be used as the large icon for display.
     * @param originalUrl             The original url of the downloaded file.
     * @param shouldPromoteOrigin     Whether the origin should be displayed in the notification.
     */
    @VisibleForTesting
    public void notifyDownloadProgress(ContentId id, String fileName, Progress progress,
            long bytesReceived, long timeRemainingInMillis, long startTime, boolean isOffTheRecord,
            boolean canDownloadWhileMetered, boolean isTransient, Bitmap icon, String originalUrl,
            boolean shouldPromoteOrigin) {
        updateActiveDownloadNotification(id, fileName, progress, timeRemainingInMillis, startTime,
                isOffTheRecord, canDownloadWhileMetered, isTransient, icon, originalUrl,
                shouldPromoteOrigin, false, PendingState.NOT_PENDING);
    }

    /**
     * Adds or updates a pending download notification.
     * @param id                      The {@link ContentId} of the download.
     * @param fileName                File name of the download.
     * @param isOffTheRecord          Whether the download is off the record.
     * @param canDownloadWhileMetered Whether the download can happen in metered network.
     * @param isTransient             Whether or not clicking on the download should launch
     *                                downloads home.
     * @param icon                    A {@link Bitmap} to be used as the large icon for display.
     * @param originalUrl             The original url of the downloaded file.
     * @param shouldPromoteOrigin     Whether the origin should be displayed in the notification.
     * @param pendingState            Reason download is pending.
     */
    void notifyDownloadPending(ContentId id, String fileName, boolean isOffTheRecord,
            boolean canDownloadWhileMetered, boolean isTransient, Bitmap icon, String originalUrl,
            boolean shouldPromoteOrigin, boolean hasUserGesture, @PendingState int pendingState) {
        updateActiveDownloadNotification(id, fileName, Progress.createIndeterminateProgress(), 0, 0,
                isOffTheRecord, canDownloadWhileMetered, isTransient, icon, originalUrl,
                shouldPromoteOrigin, hasUserGesture, pendingState);
    }

    /**
     * Helper method to update the notification for an active download, the download is either in
     * progress or pending.
     * @param id                      The {@link ContentId} of the download.
     * @param fileName                File name of the download.
     * @param progress                The current download progress.
     * @param timeRemainingInMillis   Remaining download time in milliseconds or -1 if it is
     *                                unknown.
     * @param startTime               Time when download started.
     * @param isOffTheRecord          Whether the download is off the record.
     * @param canDownloadWhileMetered Whether the download can happen in metered network.
     * @param isTransient             Whether or not clicking on the download should launch
     *                                downloads home.
     * @param icon                    A {@link Bitmap} to be used as the large icon for display.
     * @param originalUrl             The original url of the downloaded file.
     * @param shouldPromoteOrigin     Whether the origin should be displayed in the notification.
     * @param pendingState            Reason download is pending.
     */
    private void updateActiveDownloadNotification(ContentId id, String fileName, Progress progress,
            long timeRemainingInMillis, long startTime, boolean isOffTheRecord,
            boolean canDownloadWhileMetered, boolean isTransient, Bitmap icon, String originalUrl,
            boolean shouldPromoteOrigin, boolean hasUserGesture, @PendingState int pendingState) {
        int notificationId = getNotificationId(id);
        Context context = ContextUtils.getApplicationContext();

        DownloadUpdate downloadUpdate = new DownloadUpdate.Builder()
                                                .setContentId(id)
                                                .setFileName(fileName)
                                                .setProgress(progress)
                                                .setTimeRemainingInMillis(timeRemainingInMillis)
                                                .setStartTime(startTime)
                                                .setIsOffTheRecord(isOffTheRecord)
                                                .setIsTransient(isTransient)
                                                .setIcon(icon)
                                                .setOriginalUrl(originalUrl)
                                                .setShouldPromoteOrigin(shouldPromoteOrigin)
                                                .setNotificationId(notificationId)
                                                .setPendingState(pendingState)
                                                .build();
        Notification notification = DownloadNotificationFactory.buildNotification(
                context, DownloadStatus.IN_PROGRESS, downloadUpdate, notificationId);
        updateNotification(notificationId, notification, id,
                new DownloadSharedPreferenceEntry(id, notificationId, isOffTheRecord,
                        canDownloadWhileMetered, fileName, true, isTransient));

        mDownloadForegroundServiceManager.updateDownloadStatus(
                context, DownloadStatus.IN_PROGRESS, notificationId, notification);

        startTrackingInProgressDownload(id);
    }

    private void cancelNotification(int notificationId) {
        // TODO(b/65052774): Add back NOTIFICATION_NAMESPACE when able to.
        mNotificationManager.cancel(notificationId);
    }

    /**
     * Removes a download notification and all associated tracking.  This method relies on the
     * caller to provide the notification id, which is useful in the case where the internal
     * tracking doesn't exist (e.g. in the case of a successful download, where we show the download
     * completed notification and remove our internal state tracking).
     * @param notificationId Notification ID of the download
     * @param id The {@link ContentId} of the download.
     */
    public void cancelNotification(int notificationId, ContentId id) {
        cancelNotification(notificationId);
        mDownloadSharedPreferenceHelper.removeSharedPreferenceEntry(id);

        stopTrackingInProgressDownload(id);
    }

    /**
     * Called when a download is canceled.  This method uses internal tracking to try to find the
     * notification id to cancel.
     * @param id The {@link ContentId} of the download.
     */
    @VisibleForTesting
    public void notifyDownloadCanceled(ContentId id, boolean hasUserGesture) {
        DownloadSharedPreferenceEntry entry =
                mDownloadSharedPreferenceHelper.getDownloadSharedPreferenceEntry(id);
        if (entry == null) return;

        cancelNotification(entry.notificationId, id);
        mDownloadForegroundServiceManager.updateDownloadStatus(ContextUtils.getApplicationContext(),
                DownloadStatus.CANCELLED, entry.notificationId, null);
    }

    /**
     * Change a download notification to paused state.
     * @param id                  The {@link ContentId} of the download.
     * @param fileName            File name of the download.
     * @param isResumable         Whether download can be resumed.
     * @param isAutoResumable     Whether download is can be resumed automatically.
     * @param isOffTheRecord      Whether the download is off the record.
     * @param isTransient         Whether or not clicking on the download should launch downloads
     * home.
     * @param icon                A {@link Bitmap} to be used as the large icon for display.
     * @param originalUrl         The original url of the downloaded file.
     * @param shouldPromoteOrigin Whether the origin should be displayed in the notification.
     * @param forceRebuild        Whether the notification was forcibly relaunched.
     * @param pendingState        Reason download is pending.
     */
    @VisibleForTesting
    void notifyDownloadPaused(ContentId id, String fileName, boolean isResumable,
            boolean isAutoResumable, boolean isOffTheRecord, boolean isTransient, Bitmap icon,
            String originalUrl, boolean shouldPromoteOrigin, boolean hasUserGesture,
            boolean forceRebuild, @PendingState int pendingState) {
        DownloadSharedPreferenceEntry entry =
                mDownloadSharedPreferenceHelper.getDownloadSharedPreferenceEntry(id);
        if (!isResumable) {
            // TODO(cmsy): Use correct FailState.
            notifyDownloadFailed(id, fileName, icon, originalUrl, shouldPromoteOrigin,
                    isOffTheRecord, FailState.CANNOT_DOWNLOAD);
            return;
        }
        // If download is already paused, do nothing.
        if (entry != null && !entry.isAutoResumable && !forceRebuild) return;
        boolean canDownloadWhileMetered = entry == null ? false : entry.canDownloadWhileMetered;
        // If download is interrupted due to network disconnection, show download pending state.
        if (isAutoResumable || pendingState != PendingState.NOT_PENDING) {
            notifyDownloadPending(id, fileName, isOffTheRecord, canDownloadWhileMetered,
                    isTransient, icon, originalUrl, shouldPromoteOrigin, hasUserGesture,
                    pendingState);
            stopTrackingInProgressDownload(id);
            return;
        }
        int notificationId = entry == null ? getNotificationId(id) : entry.notificationId;
        Context context = ContextUtils.getApplicationContext();

        DownloadUpdate downloadUpdate = new DownloadUpdate.Builder()
                                                .setContentId(id)
                                                .setFileName(fileName)
                                                .setIsOffTheRecord(isOffTheRecord)
                                                .setIsTransient(isTransient)
                                                .setIcon(icon)
                                                .setOriginalUrl(originalUrl)
                                                .setShouldPromoteOrigin(shouldPromoteOrigin)
                                                .setNotificationId(notificationId)
                                                .build();

        Notification notification = DownloadNotificationFactory.buildNotification(
                context, DownloadStatus.PAUSED, downloadUpdate, notificationId);
        updateNotification(notificationId, notification, id,
                new DownloadSharedPreferenceEntry(id, notificationId, isOffTheRecord,
                        canDownloadWhileMetered, fileName, isAutoResumable, isTransient));

        mDownloadForegroundServiceManager.updateDownloadStatus(
                context, DownloadStatus.PAUSED, notificationId, notification);

        stopTrackingInProgressDownload(id);
    }

    /**
     * Add a download successful notification.
     * @param id                  The {@link ContentId} of the download.
     * @param filePath            Full path to the download.
     * @param fileName            Filename of the download.
     * @param systemDownloadId    Download ID assigned by system DownloadManager.
     * @param isSupportedMimeType Whether the MIME type can be viewed inside browser.
     * @param isOpenable          Whether or not this download can be opened.
     * @param icon                A {@link Bitmap} to be used as the large icon for display.
     * @param originalUrl         The original url of the downloaded file.
     * @param shouldPromoteOrigin Whether the origin should be displayed in the notification.
     * @param referrer            Referrer of the downloaded file.
     * @param totalBytes          The total number of bytes downloaded (size of file).
     * @return                    ID of the successful download notification. Used for removing the
     *                            notification when user click on the snackbar.
     */
    @VisibleForTesting
    public int notifyDownloadSuccessful(ContentId id, String filePath, String fileName,
            long systemDownloadId, boolean isOffTheRecord, boolean isSupportedMimeType,
            boolean isOpenable, Bitmap icon, String originalUrl, boolean shouldPromoteOrigin,
            String referrer, long totalBytes) {
        Context context = ContextUtils.getApplicationContext();
        int notificationId = getNotificationId(id);
        boolean needsDefaultIcon = icon == null || isOffTheRecord;
        if (mDownloadSuccessLargeIcon == null && needsDefaultIcon) {
            Bitmap bitmap =
                    BitmapFactory.decodeResource(context.getResources(), R.drawable.offline_pin);
            mDownloadSuccessLargeIcon = getLargeNotificationIcon(bitmap);
        }
        if (needsDefaultIcon) icon = mDownloadSuccessLargeIcon;

        DownloadUpdate downloadUpdate = new DownloadUpdate.Builder()
                                                .setContentId(id)
                                                .setFileName(fileName)
                                                .setFilePath(filePath)
                                                .setSystemDownload(systemDownloadId)
                                                .setIsOffTheRecord(isOffTheRecord)
                                                .setIsSupportedMimeType(isSupportedMimeType)
                                                .setIsOpenable(isOpenable)
                                                .setIcon(icon)
                                                .setNotificationId(notificationId)
                                                .setOriginalUrl(originalUrl)
                                                .setShouldPromoteOrigin(shouldPromoteOrigin)
                                                .setReferrer(referrer)
                                                .setTotalBytes(totalBytes)
                                                .build();
        Notification notification = DownloadNotificationFactory.buildNotification(
                context, DownloadStatus.COMPLETED, downloadUpdate, notificationId);

        updateNotification(notificationId, notification, id, null);
        mDownloadForegroundServiceManager.updateDownloadStatus(
                context, DownloadStatus.COMPLETED, notificationId, notification);
        stopTrackingInProgressDownload(id);
        return notificationId;
    }

    /**
     * Add a download failed notification.
     * @param id                  The {@link ContentId} of the download.
     * @param fileName            Filename of the download.
     * @param icon                A {@link Bitmap} to be used as the large icon for display.
     * @param originalUrl         The original url of the downloaded file.
     * @param shouldPromoteOrigin Whether the origin should be displayed in the notification.
     * @param isOffTheRecord      If the profile is off the record.
     * @param failState           Reason why download failed.
     */
    @VisibleForTesting
    public void notifyDownloadFailed(ContentId id, String fileName, Bitmap icon, String originalUrl,
            boolean shouldPromoteOrigin, boolean isOffTheRecord, @FailState int failState) {
        // If the download is not in history db, fileName could be empty. Get it from
        // SharedPreferences.
        if (TextUtils.isEmpty(fileName)) {
            DownloadSharedPreferenceEntry entry =
                    mDownloadSharedPreferenceHelper.getDownloadSharedPreferenceEntry(id);
            if (entry == null) return;
            fileName = entry.fileName;
        }

        int notificationId = getNotificationId(id);
        Context context = ContextUtils.getApplicationContext();

        DownloadUpdate downloadUpdate = new DownloadUpdate.Builder()
                                                .setContentId(id)
                                                .setFileName(fileName)
                                                .setIcon(icon)
                                                .setIsOffTheRecord(isOffTheRecord)
                                                .setOriginalUrl(originalUrl)
                                                .setShouldPromoteOrigin(shouldPromoteOrigin)
                                                .setFailState(failState)
                                                .build();
        Notification notification = DownloadNotificationFactory.buildNotification(
                context, DownloadStatus.FAILED, downloadUpdate, notificationId);

        updateNotification(notificationId, notification, id, null);
        mDownloadForegroundServiceManager.updateDownloadStatus(
                context, DownloadStatus.FAILED, notificationId, notification);

        stopTrackingInProgressDownload(id);
    }

    private Bitmap getLargeNotificationIcon(Bitmap bitmap) {
        Resources resources = ContextUtils.getApplicationContext().getResources();
        int height = (int) resources.getDimension(android.R.dimen.notification_large_icon_height);
        int width = (int) resources.getDimension(android.R.dimen.notification_large_icon_width);
        final OvalShape circle = new OvalShape();
        circle.resize(width, height);
        final Paint paint = new Paint();
        paint.setColor(ApiCompatibilityUtils.getColor(resources, R.color.google_blue_grey_500));

        final Bitmap result = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(result);
        circle.draw(canvas, paint);
        float leftOffset = (width - bitmap.getWidth()) / 2f;
        float topOffset = (height - bitmap.getHeight()) / 2f;
        if (leftOffset >= 0 && topOffset >= 0) {
            canvas.drawBitmap(bitmap, leftOffset, topOffset, null);
        } else {
            // Scale down the icon into the notification icon dimensions
            canvas.drawBitmap(bitmap, new Rect(0, 0, bitmap.getWidth(), bitmap.getHeight()),
                    new Rect(0, 0, width, height), null);
        }
        return result;
    }

    @VisibleForTesting
    void updateNotification(int id, Notification notification) {
        // TODO(b/65052774): Add back NOTIFICATION_NAMESPACE when able to.
        // Disabling StrictMode to avoid violations (crbug.com/789134).
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            mNotificationManager.notify(id, notification);
        }
    }

    private void updateNotification(int notificationId, Notification notification, ContentId id,
            DownloadSharedPreferenceEntry entry) {
        updateNotification(notificationId, notification);
        trackNotificationUma(id, notification);

        if (entry != null) {
            mDownloadSharedPreferenceHelper.addOrReplaceSharedPreferenceEntry(entry);
        } else {
            mDownloadSharedPreferenceHelper.removeSharedPreferenceEntry(id);
        }
    }

    private void trackNotificationUma(ContentId id, Notification notification) {
        // Check if we already have an entry in the DownloadSharedPreferenceHelper.  This is a
        // reasonable indicator for whether or not a notification is already showing (or at least if
        // we had built one for this download before.
        if (mDownloadSharedPreferenceHelper.hasEntry(id)) return;
        NotificationUmaTracker.getInstance().onNotificationShown(
                LegacyHelpers.isLegacyOfflinePage(id)
                        ? NotificationUmaTracker.SystemNotificationType.DOWNLOAD_PAGES
                        : NotificationUmaTracker.SystemNotificationType.DOWNLOAD_FILES,
                notification);
    }

    private static boolean canResumeDownload(Context context, DownloadSharedPreferenceEntry entry) {
        if (entry == null) return false;
        if (!entry.isAutoResumable) return false;

        boolean isNetworkMetered = DownloadManagerService.isActiveNetworkMetered(context);
        return entry.canDownloadWhileMetered || !isNetworkMetered;
    }

    /**
     * Resumes all pending downloads from SharedPreferences. If a download is
     * already in progress, do nothing.
     */
    void resumeAllPendingDownloads() {
        if (FeatureUtilities.isDownloadAutoResumptionEnabledInNative()) return;

        // Limit the number of auto resumption attempts in case Chrome falls into a vicious cycle.
        DownloadResumptionScheduler.getDownloadResumptionScheduler().cancel();
        int numAutoResumptionAtemptLeft = getResumptionAttemptLeft();
        if (numAutoResumptionAtemptLeft <= 0) return;

        numAutoResumptionAtemptLeft--;
        updateResumptionAttemptLeft(numAutoResumptionAtemptLeft);

        // Go through and check which downloads to resume.
        List<DownloadSharedPreferenceEntry> entries = mDownloadSharedPreferenceHelper.getEntries();
        for (int i = 0; i < entries.size(); ++i) {
            DownloadSharedPreferenceEntry entry = entries.get(i);
            if (!canResumeDownload(ContextUtils.getApplicationContext(), entry)) continue;
            if (mDownloadsInProgress.contains(entry.id)) continue;
            notifyDownloadPending(entry.id, entry.fileName, entry.isOffTheRecord,
                    entry.canDownloadWhileMetered, entry.isTransient, null, null, false, false,
                    PendingState.PENDING_NETWORK);

            Intent intent = new Intent();
            intent.setAction(ACTION_DOWNLOAD_RESUME);
            intent.putExtra(EXTRA_DOWNLOAD_CONTENTID_ID, entry.id.id);
            intent.putExtra(EXTRA_DOWNLOAD_CONTENTID_NAMESPACE, entry.id.namespace);
            intent.putExtra(EXTRA_IS_AUTO_RESUMPTION, true);

            resumeDownload(intent);
        }
    }

    @VisibleForTesting
    void resumeDownload(Intent intent) {
        DownloadBroadcastManager.startDownloadBroadcastManager(
                ContextUtils.getApplicationContext(), intent);
    }

    /**
     * Return the notification ID for the given download {@link ContentId}.
     * @param id the {@link ContentId} of the download.
     * @return notification ID to be used.
     */
    private int getNotificationId(ContentId id) {
        DownloadSharedPreferenceEntry entry =
                mDownloadSharedPreferenceHelper.getDownloadSharedPreferenceEntry(id);
        if (entry != null) return entry.notificationId;
        return getNextNotificationId();
    }

    /**
     * Get the next notificationId based on stored value and update shared preferences.
     * @return notificationId that is next based on stored value.
     */
    private static int getNextNotificationId() {
        SharedPreferences sharedPreferences = ContextUtils.getAppSharedPreferences();
        int nextNotificationId = sharedPreferences.getInt(
                KEY_NEXT_DOWNLOAD_NOTIFICATION_ID, STARTING_NOTIFICATION_ID);
        int nextNextNotificationId = nextNotificationId == Integer.MAX_VALUE
                ? STARTING_NOTIFICATION_ID
                : nextNotificationId + 1;
        SharedPreferences.Editor editor = sharedPreferences.edit();
        editor.putInt(KEY_NEXT_DOWNLOAD_NOTIFICATION_ID, nextNextNotificationId);
        editor.apply();
        return nextNotificationId;
    }

    static int getNewNotificationIdFor(int oldNotificationId) {
        int newNotificationId = getNextNotificationId();
        DownloadSharedPreferenceHelper downloadSharedPreferenceHelper =
                DownloadSharedPreferenceHelper.getInstance();
        List<DownloadSharedPreferenceEntry> entries = downloadSharedPreferenceHelper.getEntries();
        for (DownloadSharedPreferenceEntry entry : entries) {
            if (entry.notificationId == oldNotificationId) {
                DownloadSharedPreferenceEntry newEntry = new DownloadSharedPreferenceEntry(entry.id,
                        newNotificationId, entry.isOffTheRecord, entry.canDownloadWhileMetered,
                        entry.fileName, entry.isAutoResumable, entry.isTransient);
                downloadSharedPreferenceHelper.addOrReplaceSharedPreferenceEntry(
                        newEntry, true /* forceCommit */);
                break;
            }
        }
        return newNotificationId;
    }

    /**
     * Helper method to update the remaining number of background resumption attempts left.
     */
    private static void updateResumptionAttemptLeft(int numAutoResumptionAttemptLeft) {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putInt(KEY_AUTO_RESUMPTION_ATTEMPT_LEFT, numAutoResumptionAttemptLeft)
                .apply();
    }

    /**
     * Helper method to get the remaining number of background resumption attempts left.
     */
    private static int getResumptionAttemptLeft() {
        SharedPreferences sharedPrefs = ContextUtils.getAppSharedPreferences();
        return sharedPrefs.getInt(KEY_AUTO_RESUMPTION_ATTEMPT_LEFT, MAX_RESUMPTION_ATTEMPT_LEFT);
    }

    /**
     * Helper method to clear the remaining number of background resumption attempts left.
     */
    static void clearResumptionAttemptLeft() {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .remove(KEY_AUTO_RESUMPTION_ATTEMPT_LEFT)
                .apply();
    }

    void onForegroundServiceRestarted(int pinnedNotificationId) {
        // In API < 24, notifications pinned to the foreground will get killed with the service.
        // Fix this by relaunching the notification that was pinned to the service as the service
        // dies, if there is one.
        relaunchPinnedNotification(pinnedNotificationId);

        updateNotificationsForShutdown();
        resumeAllPendingDownloads();
    }

    void onForegroundServiceTaskRemoved() {
        // If we've lost all Activities, cancel the off the record downloads.
        if (ApplicationStatus.isEveryActivityDestroyed()) {
            cancelOffTheRecordDownloads();
        }
    }

    void onForegroundServiceDestroyed() {
        updateNotificationsForShutdown();
        rescheduleDownloads();
    }

    /**
     * Given the id of the notification that was pinned to the service when it died, give the
     * notification a new id in order to rebuild and relaunch the notification.
     * @param pinnedNotificationId Id of the notification pinned to the service when it died.
     */
    private void relaunchPinnedNotification(int pinnedNotificationId) {
        // If there was no notification pinned to the service, no correction is necessary.
        if (pinnedNotificationId == INVALID_NOTIFICATION_ID) return;

        List<DownloadSharedPreferenceEntry> entries = mDownloadSharedPreferenceHelper.getEntries();
        List<DownloadSharedPreferenceEntry> copies =
                new ArrayList<DownloadSharedPreferenceEntry>(entries);
        for (DownloadSharedPreferenceEntry entry : copies) {
            if (entry.notificationId == pinnedNotificationId) {
                // Get new notification id that is not associated with the service.
                DownloadSharedPreferenceEntry updatedEntry =
                        new DownloadSharedPreferenceEntry(entry.id, getNextNotificationId(),
                                entry.isOffTheRecord, entry.canDownloadWhileMetered, entry.fileName,
                                entry.isAutoResumable, entry.isTransient);
                mDownloadSharedPreferenceHelper.addOrReplaceSharedPreferenceEntry(updatedEntry);

                // Right now this only happens in the paused case, so re-build and re-launch the
                // paused notification, with the updated notification id..
                notifyDownloadPaused(updatedEntry.id, updatedEntry.fileName, true /* isResumable */,
                        updatedEntry.isAutoResumable, updatedEntry.isOffTheRecord,
                        updatedEntry.isTransient, null /* icon */, null /* originalUrl */,
                        false /* shouldPromoteOrigin */, true /* hasUserGesture */,
                        true /* forceRebuild */, PendingState.NOT_PENDING);
                return;
            }
        }
    }

    private void updateNotificationsForShutdown() {
        cancelOffTheRecordDownloads();
        List<DownloadSharedPreferenceEntry> entries = mDownloadSharedPreferenceHelper.getEntries();
        for (DownloadSharedPreferenceEntry entry : entries) {
            if (entry.isOffTheRecord) continue;
            // Move all regular downloads to pending.  Don't propagate the pause because
            // if native is still working and it triggers an update, then the service will be
            // restarted.
            notifyDownloadPaused(entry.id, entry.fileName, true, true, false, entry.isTransient,
                    null, null, false, false, false, PendingState.PENDING_NETWORK);
        }
    }

    private void cancelOffTheRecordDownloads() {
        boolean cancelActualDownload =
                BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER)
                        .isFullBrowserStarted()
                && Profile.getLastUsedProfile().hasOffTheRecordProfile();

        List<DownloadSharedPreferenceEntry> entries = mDownloadSharedPreferenceHelper.getEntries();
        List<DownloadSharedPreferenceEntry> copies =
                new ArrayList<DownloadSharedPreferenceEntry>(entries);
        for (DownloadSharedPreferenceEntry entry : copies) {
            if (!entry.isOffTheRecord) continue;
            ContentId id = entry.id;
            notifyDownloadCanceled(id, false);
            if (cancelActualDownload) {
                DownloadServiceDelegate delegate = getServiceDelegate(id);
                delegate.cancelDownload(id, true);
                delegate.destroyServiceDelegate();
            }
        }
    }

    private void rescheduleDownloads() {
        if (getResumptionAttemptLeft() <= 0) return;
        DownloadResumptionScheduler.getDownloadResumptionScheduler().scheduleIfNecessary();
    }
}
