// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static org.chromium.chrome.browser.notifications.NotificationConstants.DEFAULT_NOTIFICATION_ID;

import android.app.Notification;
import android.content.Intent;
import android.graphics.Bitmap;

import org.junit.Assert;

import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.FailState;
import org.chromium.components.offline_items_collection.OfflineItem.Progress;
import org.chromium.components.offline_items_collection.PendingState;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.ArrayList;
import java.util.List;

/**
 * Mock class to DownloadNotificationService for testing purpose.
 */
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
    public int notifyDownloadSuccessful(final ContentId id, final String filePath,
            final String fileName, final long systemDownloadId, final boolean isOffTheRecord,
            final boolean isSupportedMimeType, final boolean isOpenable, final Bitmap icon,
            final String originalUrl, final boolean shouldPromoteOrigin, final String referrer,
            final long totalBytes) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                ()
                        -> MockDownloadNotificationService.super.notifyDownloadSuccessful(id,
                                filePath, fileName, systemDownloadId, isOffTheRecord,
                                isSupportedMimeType, isOpenable, icon, originalUrl,
                                shouldPromoteOrigin, referrer, totalBytes));
    }

    @Override
    public void notifyDownloadProgress(final ContentId id, final String fileName,
            final Progress progress, final long bytesReceived, final long timeRemainingInMillis,
            final long startTime, final boolean isOffTheRecord,
            final boolean canDownloadWhileMetered, final boolean isTransient, final Bitmap icon,
            final String originalUrl, final boolean shouldPromoteOrigin) {
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> MockDownloadNotificationService.super.notifyDownloadProgress(id,
                                fileName, progress, bytesReceived, timeRemainingInMillis, startTime,
                                isOffTheRecord, canDownloadWhileMetered, isTransient, icon,
                                originalUrl, shouldPromoteOrigin));
    }

    @Override
    void notifyDownloadPaused(ContentId id, String fileName, boolean isResumable,
            boolean isAutoResumable, boolean isOffTheRecord, boolean isTransient, Bitmap icon,
            final String originalUrl, final boolean shouldPromoteOrigin, boolean hasUserGesture,
            boolean forceRebuild, @PendingState int pendingState) {
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> MockDownloadNotificationService.super.notifyDownloadPaused(id, fileName,
                                isResumable, isAutoResumable, isOffTheRecord, isTransient, icon,
                                originalUrl, shouldPromoteOrigin, hasUserGesture, forceRebuild,
                                pendingState));
    }

    @Override
    public void notifyDownloadFailed(final ContentId id, final String fileName, final Bitmap icon,
            final String originalUrl, final boolean shouldPromoteOrigin, boolean isOffTheRecord,
            @FailState int failState) {
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> MockDownloadNotificationService.super.notifyDownloadFailed(id, fileName,
                                icon, originalUrl, shouldPromoteOrigin, isOffTheRecord, failState));
    }

    @Override
    public void notifyDownloadCanceled(final ContentId id, boolean hasUserGesture) {
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> MockDownloadNotificationService.super.notifyDownloadCanceled(
                                id, hasUserGesture));
    }

    @Override
    void resumeDownload(Intent intent) {
        mResumedDownloads.add(IntentUtils.safeGetStringExtra(intent, EXTRA_DOWNLOAD_CONTENTID_ID));
        Assert.assertTrue(IntentUtils.safeGetBooleanExtra(intent, EXTRA_IS_AUTO_RESUMPTION, false));
    }
}
