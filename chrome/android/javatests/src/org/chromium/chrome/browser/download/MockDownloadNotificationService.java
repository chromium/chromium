// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static org.chromium.chrome.browser.notifications.NotificationConstants.DEFAULT_NOTIFICATION_ID;

import android.app.Notification;
import android.content.Intent;
import android.graphics.Bitmap;

import org.chromium.base.IntentUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.FailState;
import org.chromium.components.offline_items_collection.OfflineItem.Progress;
import org.chromium.components.offline_items_collection.PendingState;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** Mock class to DownloadNotificationService for testing purpose. */
public class MockDownloadNotificationService extends DownloadNotificationService {
    private final List<Integer> mNotificationIds = new ArrayList<Integer>();
    private boolean mPaused;
    private int mLastNotificationId = DEFAULT_NOTIFICATION_ID;
    private int mNumberOfNotifications;

    List<String> mResumedDownloads = new ArrayList<>();

    @Override
    void updateNotification(int id, Notification notification) {
        mNumberOfNotifications++;
        mLastNotificationId = id;
        if (!mNotificationIds.contains(id)) mNotificationIds.add(id);
    }

    public boolean isPaused() {
        return mPaused;
    }

    public List<Integer> getNotificationIds() {
        return mNotificationIds;
    }

    public int getLastNotificationId() {
        return mLastNotificationId;
    }

    public int getNumberOfNotifications() {
        return mNumberOfNotifications;
    }

    @Override
    public void cancelNotification(int notificationId, ContentId id) {
        super.cancelNotification(notificationId, id);
        mNotificationIds.remove(Integer.valueOf(notificationId));
    }

    @Override
    public int notifyDownloadSuccessful(
            final ContentId id,
            final String filePath,
            final String fileName,
            final long systemDownloadId,
            final OTRProfileID otrProfileID,
            final boolean isSupportedMimeType,
            final boolean isOpenable,
            final Bitmap icon,
            final GURL originalUrl,
            final boolean shouldPromoteOrigin,
            final GURL referrer,
            final long totalBytes) {
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        MockDownloadNotificationService.super.notifyDownloadSuccessful(
                                id,
                                filePath,
                                fileName,
                                systemDownloadId,
                                otrProfileID,
                                isSupportedMimeType,
                                isOpenable,
                                icon,
                                originalUrl,
                                shouldPromoteOrigin,
                                referrer,
                                totalBytes));
    }

    @Override
    public void notifyDownloadProgress(
            final ContentId id,
            final String fileName,
            final Progress progress,
            final long bytesReceived,
            final long timeRemainingInMillis,
            final long startTime,
            final OTRProfileID otrProfileID,
            final boolean canDownloadWhileMetered,
            final boolean isTransient,
            final Bitmap icon,
            final GURL originalUrl,
            final boolean shouldPromoteOrigin) {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        MockDownloadNotificationService.super.notifyDownloadProgress(
                                id,
                                fileName,
                                progress,
                                bytesReceived,
                                timeRemainingInMillis,
                                startTime,
                                otrProfileID,
                                canDownloadWhileMetered,
                                isTransient,
                                icon,
                                originalUrl,
                                shouldPromoteOrigin));
    }

    @Override
    void notifyDownloadPaused(
            ContentId id,
            String fileName,
            boolean isResumable,
            boolean isAutoResumable,
            OTRProfileID otrProfileID,
            boolean isTransient,
            Bitmap icon,
            final GURL originalUrl,
            final boolean shouldPromoteOrigin,
            boolean hasUserGesture,
            boolean forceRebuild,
            @PendingState int pendingState) {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        MockDownloadNotificationService.super.notifyDownloadPaused(
                                id,
                                fileName,
                                isResumable,
                                isAutoResumable,
                                otrProfileID,
                                isTransient,
                                icon,
                                originalUrl,
                                shouldPromoteOrigin,
                                hasUserGesture,
                                forceRebuild,
                                pendingState));
    }

    @Override
    public void notifyDownloadFailed(
            final ContentId id,
            final String fileName,
            final Bitmap icon,
            final GURL originalUrl,
            final boolean shouldPromoteOrigin,
            OTRProfileID otrProfileID,
            @FailState int failState) {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        MockDownloadNotificationService.super.notifyDownloadFailed(
                                id,
                                fileName,
                                icon,
                                originalUrl,
                                shouldPromoteOrigin,
                                otrProfileID,
                                failState));
    }

    @Override
    public void notifyDownloadCanceled(final ContentId id, boolean hasUserGesture) {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        MockDownloadNotificationService.super.notifyDownloadCanceled(
                                id, hasUserGesture));
    }

    @Override
    void resumeDownload(Intent intent) {
        mResumedDownloads.add(IntentUtils.safeGetStringExtra(intent, EXTRA_DOWNLOAD_CONTENTID_ID));
    }
}
