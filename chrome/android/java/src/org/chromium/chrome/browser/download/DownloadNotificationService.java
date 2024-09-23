// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static org.chromium.chrome.browser.download.DownloadBroadcastManagerImpl.getServiceDelegate;
import static org.chromium.chrome.browser.download.DownloadSnackbarController.INVALID_NOTIFICATION_ID;

import android.app.Notification;
import android.content.Context;
import android.content.Intent;
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

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.components.background_task_scheduler.BackgroundTask.TaskFinishedCallback;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.FailState;
import org.chromium.components.offline_items_collection.LegacyHelpers;
import org.chromium.components.offline_items_collection.OfflineItem.Progress;
import org.chromium.components.offline_items_collection.PendingState;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.url.GURL;

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
    @IntDef({
        DownloadStatus.IN_PROGRESS,
        DownloadStatus.PAUSED,
        DownloadStatus.COMPLETED,
        DownloadStatus.CANCELLED,
        DownloadStatus.FAILED
    })
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
    static final String EXTRA_OTR_PROFILE_ID =
            "org.chromium.chrome.browser.download.OTR_PROFILE_ID";

    static final String EXTRA_NOTIFICATION_BUNDLE_ICON_ID = "Chrome.NotificationBundleIconIdExtra";

    /** Notification Id starting value, to avoid conflicts from IDs used in prior versions. */
    private static final int STARTING_NOTIFICATION_ID = 1000000;

    private static final int MAX_RESUMPTION_ATTEMPT_LEFT = 5;

    private static DownloadNotificationService sInstanceForTesting;

    private BaseNotificationManagerProxy mNotificationManager;
    private Bitmap mDownloadSuccessLargeIcon;
    private DownloadSharedPreferenceHelper mDownloadSharedPreferenceHelper;
    private DownloadForegroundServiceManager mDownloadForegroundServiceManager;
    private DownloadUserInitiatedTaskManager mDownloadUserInitiatedTaskManager;

    private static class LazyHolder {
        private static final DownloadNotificationService INSTANCE =
                new DownloadNotificationService();
    }

    /** Creates DownloadNotificationService. */
    public static DownloadNotificationService getInstance() {
        return sInstanceForTesting == null ? LazyHolder.INSTANCE : sInstanceForTesting;
    }

    public static void setInstanceForTests(DownloadNotificationService service) {
        sInstanceForTesting = service;
        ResettersForTesting.register(() -> sInstanceForTesting = null);
    }

    @VisibleForTesting
    DownloadNotificationService() {
        mNotificationManager =
                BaseNotificationManagerProxyFactory.create(ContextUtils.getApplicationContext());
        mDownloadSharedPreferenceHelper = DownloadSharedPreferenceHelper.getInstance();
        mDownloadForegroundServiceManager = new DownloadForegroundServiceManager();
        mDownloadUserInitiatedTaskManager = new DownloadUserInitiatedTaskManager();
    }

    /**
     * Called to set a callback that will be run to attach a notification to the background task
     * life-cycle.
     *
     * @param backgroundTaskNotificationCallback The callback to be invoked to attach
     *     notification.
     */
    public void setBackgroundTaskNotificationCallback(
            int taskId, TaskFinishedCallback backgroundTaskNotificationCallback) {
        mDownloadUserInitiatedTaskManager.setTaskNotificationCallback(
                taskId, backgroundTaskNotificationCallback);
    }

    @VisibleForTesting
    void setDownloadForegroundServiceManager(
            DownloadForegroundServiceManager downloadForegroundServiceManager) {
        mDownloadForegroundServiceManager = downloadForegroundServiceManager;
    }

    /**
     * Adds or updates an in-progress download notification.
     * @param id                      The {@link ContentId} of the download.
     * @param fileName                File name of the download.
     * @param progress                The current download progress.
     * @param bytesReceived           Total number of bytes received.
     * @param timeRemainingInMillis   Remaining download time in milliseconds.
     * @param startTime               Time when download started.
     * @param otrProfileID            The {@link OTRProfileID} of the download. Null if in regular
     *                                mode.
     * @param canDownloadWhileMetered Whether the download can happen in metered network.
     * @param isTransient             Whether or not clicking on the download should launch
     *                                downloads home.
     * @param icon                    A {@link Bitmap} to be used as the large icon for display.
     * @param originalUrl             The original url of the downloaded file.
     * @param shouldPromoteOrigin     Whether the origin should be displayed in the notification.
     */
    @VisibleForTesting
    public void notifyDownloadProgress(
            ContentId id,
            String fileName,
            Progress progress,
            long bytesReceived,
            long timeRemainingInMillis,
            long startTime,
            OTRProfileID otrProfileID,
            boolean canDownloadWhileMetered,
            boolean isTransient,
            Bitmap icon,
            GURL originalUrl,
            boolean shouldPromoteOrigin) {
        updateActiveDownloadNotification(
                id,
                fileName,
                progress,
                timeRemainingInMillis,
                startTime,
                otrProfileID,
                canDownloadWhileMetered,
                isTransient,
                icon,
                originalUrl,
                shouldPromoteOrigin,
                false,
                PendingState.NOT_PENDING);
    }

    /**
     * Adds or updates a pending download notification.
     * @param id                      The {@link ContentId} of the download.
     * @param fileName                File name of the download.
     * @param otrProfileID            The {@link OTRProfileID} of the download. Null if in regular
     *                                mode.
     * @param canDownloadWhileMetered Whether the download can happen in metered network.
     * @param isTransient             Whether or not clicking on the download should launch
     *                                downloads home.
     * @param icon                    A {@link Bitmap} to be used as the large icon for display.
     * @param originalUrl             The original url of the downloaded file.
     * @param shouldPromoteOrigin     Whether the origin should be displayed in the notification.
     * @param pendingState            Reason download is pending.
     */
    void notifyDownloadPending(
            ContentId id,
            String fileName,
            OTRProfileID otrProfileID,
            boolean canDownloadWhileMetered,
            boolean isTransient,
            Bitmap icon,
            GURL originalUrl,
            boolean shouldPromoteOrigin,
            boolean hasUserGesture,
            @PendingState int pendingState) {
        updateActiveDownloadNotification(
                id,
                fileName,
                Progress.createIndeterminateProgress(),
                0,
                0,
                otrProfileID,
                canDownloadWhileMetered,
                isTransient,
                icon,
                originalUrl,
                shouldPromoteOrigin,
                hasUserGesture,
                pendingState);
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
     * @param otrProfileID            The {@link OTRProfileID} of the download. Null if in regular
     *                                mode.
     * @param canDownloadWhileMetered Whether the download can happen in metered network.
     * @param isTransient             Whether or not clicking on the download should launch
     *                                downloads home.
     * @param icon                    A {@link Bitmap} to be used as the large icon for display.
     * @param originalUrl             The original url of the downloaded file.
     * @param shouldPromoteOrigin     Whether the origin should be displayed in the notification.
     * @param pendingState            Reason download is pending.
     */
    private void updateActiveDownloadNotification(
            ContentId id,
            String fileName,
            Progress progress,
            long timeRemainingInMillis,
            long startTime,
            OTRProfileID otrProfileID,
            boolean canDownloadWhileMetered,
            boolean isTransient,
            Bitmap icon,
            GURL originalUrl,
            boolean shouldPromoteOrigin,
            boolean hasUserGesture,
            @PendingState int pendingState) {
        int notificationId = getNotificationId(id);
        Context context = ContextUtils.getApplicationContext();

        DownloadUpdate downloadUpdate =
                new DownloadUpdate.Builder()
                        .setContentId(id)
                        .setFileName(fileName)
                        .setProgress(progress)
                        .setTimeRemainingInMillis(timeRemainingInMillis)
                        .setStartTime(startTime)
                        .setOTRProfileID(otrProfileID)
                        .setIsTransient(isTransient)
                        .setIcon(icon)
                        .setOriginalUrl(originalUrl)
                        .setShouldPromoteOrigin(shouldPromoteOrigin)
                        .setNotificationId(notificationId)
                        .setPendingState(pendingState)
                        .build();
        Notification notification =
                DownloadNotificationFactory.buildNotification(
                        context, DownloadStatus.IN_PROGRESS, downloadUpdate, notificationId);
        updateNotification(
                notificationId,
                notification,
                id,
                new DownloadSharedPreferenceEntry(
                        id,
                        notificationId,
                        otrProfileID,
                        canDownloadWhileMetered,
                        fileName,
                        true,
                        isTransient));
        mDownloadForegroundServiceManager.updateDownloadStatus(
                context, DownloadStatus.IN_PROGRESS, notificationId, notification);
        mDownloadUserInitiatedTaskManager.updateDownloadStatus(
                context, DownloadStatus.IN_PROGRESS, notificationId, notification);
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
    }

    /**
     * Called when a download is canceled given the notification ID.
     * @param id The {@link ContentId} of the download.
     * @param notificationId Notification ID of the download.
     * @param hasUserGesture Whether cancel is triggered by user gesture.
     */
    @VisibleForTesting
    public void notifyDownloadCanceled(ContentId id, int notificationId, boolean hasUserGesture) {
        mDownloadForegroundServiceManager.updateDownloadStatus(
                ContextUtils.getApplicationContext(),
                DownloadStatus.CANCELLED,
                notificationId,
                null);
        mDownloadUserInitiatedTaskManager.updateDownloadStatus(
                ContextUtils.getApplicationContext(),
                DownloadStatus.CANCELLED,
                notificationId,
                null);
        cancelNotification(notificationId, id);
    }

    /**
     * Called when a download is canceled.  This method uses internal tracking to try to find the
     * notification id to cancel.
     * Called when a download is canceled.
     * @param id The {@link ContentId} of the download.
     * @param hasUserGesture Whether cancel is triggered by user gesture.
     */
    @VisibleForTesting
    public void notifyDownloadCanceled(ContentId id, boolean hasUserGesture) {
        DownloadSharedPreferenceEntry entry =
                mDownloadSharedPreferenceHelper.getDownloadSharedPreferenceEntry(id);
        if (entry == null) return;
        notifyDownloadCanceled(id, entry.notificationId, hasUserGesture);
    }

    /**
     * Change a download notification to paused state.
     * @param id                  The {@link ContentId} of the download.
     * @param fileName            File name of the download.
     * @param isResumable         Whether download can be resumed.
     * @param isAutoResumable     Whether download is can be resumed automatically.
     * @param otrProfileID        The {@link OTRProfileID} of the download. Null if in regular mode.
     * @param isTransient         Whether or not clicking on the download should launch downloads
     * home.
     * @param icon                A {@link Bitmap} to be used as the large icon for display.
     * @param originalUrl         The original url of the downloaded file.
     * @param shouldPromoteOrigin Whether the origin should be displayed in the notification.
     * @param forceRebuild        Whether the notification was forcibly relaunched.
     * @param pendingState        Reason download is pending.
     */
    @VisibleForTesting
    void notifyDownloadPaused(
            ContentId id,
            String fileName,
            boolean isResumable,
            boolean isAutoResumable,
            OTRProfileID otrProfileID,
            boolean isTransient,
            Bitmap icon,
            GURL originalUrl,
            boolean shouldPromoteOrigin,
            boolean hasUserGesture,
            boolean forceRebuild,
            @PendingState int pendingState) {
        DownloadSharedPreferenceEntry entry =
                mDownloadSharedPreferenceHelper.getDownloadSharedPreferenceEntry(id);
        if (!isResumable) {
            // TODO(cmsy): Use correct FailState.
            notifyDownloadFailed(
                    id,
                    fileName,
                    icon,
                    originalUrl,
                    shouldPromoteOrigin,
                    otrProfileID,
                    FailState.CANNOT_DOWNLOAD);
            return;
        }
        // If download is already paused, do nothing.
        if (entry != null && !entry.isAutoResumable && !forceRebuild) return;
        boolean canDownloadWhileMetered = entry == null ? false : entry.canDownloadWhileMetered;
        // If download is interrupted due to network disconnection, show download pending state.
        if (isAutoResumable || pendingState != PendingState.NOT_PENDING) {
            notifyDownloadPending(
                    id,
                    fileName,
                    otrProfileID,
                    canDownloadWhileMetered,
                    isTransient,
                    icon,
                    originalUrl,
                    shouldPromoteOrigin,
                    hasUserGesture,
                    pendingState);
            return;
        }
        int notificationId = entry == null ? getNotificationId(id) : entry.notificationId;
        Context context = ContextUtils.getApplicationContext();

        DownloadUpdate downloadUpdate =
                new DownloadUpdate.Builder()
                        .setContentId(id)
                        .setFileName(fileName)
                        .setOTRProfileID(otrProfileID)
                        .setIsTransient(isTransient)
                        .setIcon(icon)
                        .setOriginalUrl(originalUrl)
                        .setShouldPromoteOrigin(shouldPromoteOrigin)
                        .setNotificationId(notificationId)
                        .build();

        Notification notification =
                DownloadNotificationFactory.buildNotification(
                        context, DownloadStatus.PAUSED, downloadUpdate, notificationId);
        updateNotification(
                notificationId,
                notification,
                id,
                new DownloadSharedPreferenceEntry(
                        id,
                        notificationId,
                        otrProfileID,
                        canDownloadWhileMetered,
                        fileName,
                        isAutoResumable,
                        isTransient));

        mDownloadForegroundServiceManager.updateDownloadStatus(
                context, DownloadStatus.PAUSED, notificationId, notification);
        mDownloadUserInitiatedTaskManager.updateDownloadStatus(
                context, DownloadStatus.PAUSED, notificationId, notification);
    }

    /**
     * Add a download successful notification.
     * @param id                  The {@link ContentId} of the download.
     * @param filePath            Full path to the download.
     * @param fileName            Filename of the download.
     * @param systemDownloadId    Download ID assigned by system DownloadManager.
     * @param otrProfileID        The {@link OTRProfileID} of the download. Null if in regular mode.
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
    public int notifyDownloadSuccessful(
            ContentId id,
            String filePath,
            String fileName,
            long systemDownloadId,
            OTRProfileID otrProfileID,
            boolean isSupportedMimeType,
            boolean isOpenable,
            Bitmap icon,
            GURL originalUrl,
            boolean shouldPromoteOrigin,
            GURL referrer,
            long totalBytes) {
        Context context = ContextUtils.getApplicationContext();
        int notificationId = getNotificationId(id);
        boolean needsDefaultIcon = icon == null || OTRProfileID.isOffTheRecord(otrProfileID);
        if (mDownloadSuccessLargeIcon == null && needsDefaultIcon) {
            Bitmap bitmap =
                    BitmapFactory.decodeResource(context.getResources(), R.drawable.offline_pin);
            mDownloadSuccessLargeIcon = getLargeNotificationIcon(bitmap);
        }
        if (needsDefaultIcon) icon = mDownloadSuccessLargeIcon;
        DownloadUpdate downloadUpdate =
                new DownloadUpdate.Builder()
                        .setContentId(id)
                        .setFileName(fileName)
                        .setFilePath(filePath)
                        .setSystemDownload(systemDownloadId)
                        .setOTRProfileID(otrProfileID)
                        .setIsSupportedMimeType(isSupportedMimeType)
                        .setIsOpenable(isOpenable)
                        .setIcon(icon)
                        .setNotificationId(notificationId)
                        .setOriginalUrl(originalUrl)
                        .setShouldPromoteOrigin(shouldPromoteOrigin)
                        .setReferrer(referrer)
                        .setTotalBytes(totalBytes)
                        .build();
        Notification notification =
                DownloadNotificationFactory.buildNotification(
                        context, DownloadStatus.COMPLETED, downloadUpdate, notificationId);

        updateNotification(notificationId, notification, id, null);
        mDownloadForegroundServiceManager.updateDownloadStatus(
                context, DownloadStatus.COMPLETED, notificationId, notification);
        mDownloadUserInitiatedTaskManager.updateDownloadStatus(
                context, DownloadStatus.COMPLETED, notificationId, notification);
        return notificationId;
    }

    /**
     * Add a download failed notification.
     * @param id                  The {@link ContentId} of the download.
     * @param fileName            Filename of the download.
     * @param icon                A {@link Bitmap} to be used as the large icon for display.
     * @param originalUrl         The original url of the downloaded file.
     * @param shouldPromoteOrigin Whether the origin should be displayed in the notification.
     * @param otrProfileID        The {@link OTRProfileID} of the download. Null if in regular mode.
     * @param failState           Reason why download failed.
     */
    @VisibleForTesting
    public void notifyDownloadFailed(
            ContentId id,
            String fileName,
            Bitmap icon,
            GURL originalUrl,
            boolean shouldPromoteOrigin,
            OTRProfileID otrProfileID,
            @FailState int failState) {
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

        DownloadUpdate downloadUpdate =
                new DownloadUpdate.Builder()
                        .setContentId(id)
                        .setFileName(fileName)
                        .setIcon(icon)
                        .setOTRProfileID(otrProfileID)
                        .setOriginalUrl(originalUrl)
                        .setShouldPromoteOrigin(shouldPromoteOrigin)
                        .setFailState(failState)
                        .build();
        Notification notification =
                DownloadNotificationFactory.buildNotification(
                        context, DownloadStatus.FAILED, downloadUpdate, notificationId);

        updateNotification(notificationId, notification, id, null);
        mDownloadForegroundServiceManager.updateDownloadStatus(
                context, DownloadStatus.FAILED, notificationId, notification);
        mDownloadUserInitiatedTaskManager.updateDownloadStatus(
                context, DownloadStatus.FAILED, notificationId, notification);
    }

    private Bitmap getLargeNotificationIcon(Bitmap bitmap) {
        Resources resources = ContextUtils.getApplicationContext().getResources();
        int height = (int) resources.getDimension(android.R.dimen.notification_large_icon_height);
        int width = (int) resources.getDimension(android.R.dimen.notification_large_icon_width);
        final OvalShape circle = new OvalShape();
        circle.resize(width, height);
        final Paint paint = new Paint();
        paint.setColor(ContextUtils.getApplicationContext().getColor(R.color.google_blue_grey_500));

        final Bitmap result = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(result);
        circle.draw(canvas, paint);
        float leftOffset = (width - bitmap.getWidth()) / 2f;
        float topOffset = (height - bitmap.getHeight()) / 2f;
        if (leftOffset >= 0 && topOffset >= 0) {
            canvas.drawBitmap(bitmap, leftOffset, topOffset, null);
        } else {
            // Scale down the icon into the notification icon dimensions
            canvas.drawBitmap(
                    bitmap,
                    new Rect(0, 0, bitmap.getWidth(), bitmap.getHeight()),
                    new Rect(0, 0, width, height),
                    null);
        }
        return result;
    }

    @VisibleForTesting
    void updateNotification(int id, Notification notification) {
        // TODO(b/65052774): Add back NOTIFICATION_NAMESPACE when able to.
        mNotificationManager.notify(
                new NotificationWrapper(
                        notification,
                        new NotificationMetadata(
                                NotificationUmaTracker.SystemNotificationType.DOWNLOAD_FILES,
                                /* tag= */ null,
                                id)));
    }

    private void updateNotification(
            int notificationId,
            Notification notification,
            ContentId id,
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
        NotificationUmaTracker.getInstance()
                .onNotificationShown(
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

    @VisibleForTesting
    void resumeDownload(Intent intent) {
        DownloadBroadcastManagerImpl.startDownloadBroadcastManager(
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
        int nextNotificationId =
                ChromeSharedPreferences.getInstance()
                        .readInt(
                                ChromePreferenceKeys.DOWNLOAD_NEXT_DOWNLOAD_NOTIFICATION_ID,
                                STARTING_NOTIFICATION_ID);
        int nextNextNotificationId =
                nextNotificationId == Integer.MAX_VALUE
                        ? STARTING_NOTIFICATION_ID
                        : nextNotificationId + 1;
        ChromeSharedPreferences.getInstance()
                .writeInt(
                        ChromePreferenceKeys.DOWNLOAD_NEXT_DOWNLOAD_NOTIFICATION_ID,
                        nextNextNotificationId);
        return nextNotificationId;
    }

    static int getNewNotificationIdFor(int oldNotificationId) {
        int newNotificationId = getNextNotificationId();
        DownloadSharedPreferenceHelper downloadSharedPreferenceHelper =
                DownloadSharedPreferenceHelper.getInstance();
        List<DownloadSharedPreferenceEntry> entries = downloadSharedPreferenceHelper.getEntries();
        for (DownloadSharedPreferenceEntry entry : entries) {
            if (entry.notificationId == oldNotificationId) {
                DownloadSharedPreferenceEntry newEntry =
                        new DownloadSharedPreferenceEntry(
                                entry.id,
                                newNotificationId,
                                entry.otrProfileID,
                                entry.canDownloadWhileMetered,
                                entry.fileName,
                                entry.isAutoResumable,
                                entry.isTransient);
                downloadSharedPreferenceHelper.addOrReplaceSharedPreferenceEntry(
                        newEntry, /* forceCommit= */ true);
                break;
            }
        }
        return newNotificationId;
    }

    void onForegroundServiceTaskRemoved() {
        // If we've lost all Activities, cancel the off the record downloads.
        if (ApplicationStatus.isEveryActivityDestroyed()) {
            cancelOffTheRecordDownloads();
        }
    }

    void onForegroundServiceDestroyed() {
        updateNotificationsForShutdown();
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
                        new DownloadSharedPreferenceEntry(
                                entry.id,
                                getNextNotificationId(),
                                entry.otrProfileID,
                                entry.canDownloadWhileMetered,
                                entry.fileName,
                                entry.isAutoResumable,
                                entry.isTransient);
                mDownloadSharedPreferenceHelper.addOrReplaceSharedPreferenceEntry(updatedEntry);

                // Right now this only happens in the paused case, so re-build and re-launch the
                // paused notification, with the updated notification id..
                notifyDownloadPaused(
                        updatedEntry.id,
                        updatedEntry.fileName,
                        /* isResumable= */ true,
                        updatedEntry.isAutoResumable,
                        updatedEntry.otrProfileID,
                        updatedEntry.isTransient,
                        /* icon= */ null,
                        /* originalUrl= */ null,
                        /* shouldPromoteOrigin= */ false,
                        /* hasUserGesture= */ true,
                        /* forceRebuild= */ true,
                        PendingState.NOT_PENDING);
                return;
            }
        }
    }

    private void updateNotificationsForShutdown() {
        cancelOffTheRecordDownloads();
        List<DownloadSharedPreferenceEntry> entries = mDownloadSharedPreferenceHelper.getEntries();
        for (DownloadSharedPreferenceEntry entry : entries) {
            if (OTRProfileID.isOffTheRecord(entry.otrProfileID)) continue;
            // Move all regular downloads to pending.  Don't propagate the pause because
            // if native is still working and it triggers an update, then the service will be
            // restarted.
            notifyDownloadPaused(
                    entry.id,
                    entry.fileName,
                    true,
                    true,
                    null,
                    entry.isTransient,
                    null,
                    null,
                    false,
                    false,
                    false,
                    PendingState.PENDING_NETWORK);
        }
    }

    public void cancelOffTheRecordDownloads() {
        boolean cancelActualDownload =
                BrowserStartupController.getInstance().isFullBrowserStarted()
                        && ProfileManager.getLastUsedRegularProfile().hasPrimaryOTRProfile();

        List<DownloadSharedPreferenceEntry> entries = mDownloadSharedPreferenceHelper.getEntries();
        List<DownloadSharedPreferenceEntry> copies =
                new ArrayList<DownloadSharedPreferenceEntry>(entries);
        for (DownloadSharedPreferenceEntry entry : copies) {
            if (!OTRProfileID.isOffTheRecord(entry.otrProfileID)) continue;
            ContentId id = entry.id;
            notifyDownloadCanceled(id, false);
            if (cancelActualDownload) {
                DownloadServiceDelegate delegate = getServiceDelegate(id);
                delegate.cancelDownload(id, entry.otrProfileID);
                delegate.destroyServiceDelegate();
            }
        }
    }
}
