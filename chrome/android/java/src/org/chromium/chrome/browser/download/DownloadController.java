// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.Manifest.permission;
import android.app.Activity;
import android.content.pm.PackageManager;
import android.util.Pair;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.download.DownloadCollectionBridge;
import org.chromium.components.permissions.AndroidPermissionRequester;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.ui.base.AndroidPermissionDelegate;
import org.chromium.ui.base.PermissionCallback;
import org.chromium.ui.base.WindowAndroid;

/**
 * Java counterpart of android DownloadController. Owned by native.
 */
public class DownloadController {
    /**
     * Class for notifying download events to other classes.
     */
    public interface Observer {
        /**
         * Notify the host application that a download is finished.
         * @param downloadInfo Information about the completed download.
         */
        void onDownloadCompleted(final DownloadInfo downloadInfo);

        /**
         * Notify the host application that a download is in progress.
         * @param downloadInfo Information about the in-progress download.
         */
        void onDownloadUpdated(final DownloadInfo downloadInfo);

        /**
         * Notify the host application that a download is cancelled.
         * @param downloadInfo Information about the cancelled download.
         */
        void onDownloadCancelled(final DownloadInfo downloadInfo);

        /**
         * Notify the host application that a download is interrupted.
         * @param downloadInfo Information about the completed download.
         * @param isAutoResumable Download can be auto resumed when network becomes available.
         */
        void onDownloadInterrupted(final DownloadInfo downloadInfo, boolean isAutoResumable);
    }

    private static Observer sObserver;

    public static void setDownloadNotificationService(Observer observer) {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.DOWNLOAD_OFFLINE_CONTENT_PROVIDER)) {
            return;
        }

        sObserver = observer;
    }

    /**
     * Notifies the download delegate that a download completed and passes along info about the
     * download. This can be either a POST download or a GET download with authentication.
     */
    @CalledByNative
    private static void onDownloadCompleted(DownloadInfo downloadInfo) {
        DownloadMetrics.recordDownloadDirectoryType(downloadInfo.getFilePath());
        MediaStoreHelper.addImageToGalleryOnSDCard(
                downloadInfo.getFilePath(), downloadInfo.getMimeType());

        if (sObserver == null) return;
        sObserver.onDownloadCompleted(downloadInfo);
    }

    /**
     * Notifies the download delegate that a download completed and passes along info about the
     * download. This can be either a POST download or a GET download with authentication.
     */
    @CalledByNative
    private static void onDownloadInterrupted(DownloadInfo downloadInfo, boolean isAutoResumable) {
        if (sObserver == null) return;
        sObserver.onDownloadInterrupted(downloadInfo, isAutoResumable);
    }

    /**
     * Called when a download was cancelled.
     */
    @CalledByNative
    private static void onDownloadCancelled(DownloadInfo downloadInfo) {
        if (sObserver == null) return;
        sObserver.onDownloadCancelled(downloadInfo);
    }

    /**
     * Notifies the download delegate about progress of a download. Downloads that use Chrome
     * network stack use custom notification to display the progress of downloads.
     */
    @CalledByNative
    private static void onDownloadUpdated(DownloadInfo downloadInfo) {
        if (sObserver == null) return;
        sObserver.onDownloadUpdated(downloadInfo);
    }


    /**
     * Returns whether file access is allowed.
     *
     * @return true if allowed, or false otherwise.
     */
    @CalledByNative
    private static boolean hasFileAccess() {
        if (DownloadCollectionBridge.supportsDownloadCollection()) return true;
        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        if (activity instanceof ChromeActivity) {
            return ((ChromeActivity) activity)
                    .getWindowAndroid()
                    .hasPermission(permission.WRITE_EXTERNAL_STORAGE);
        }
        return false;
    }

    /**
     * Requests the stoarge permission. This should be called from the native code.
     * @param callbackId ID of native callback to notify the result.
     */
    @CalledByNative
    private static void requestFileAccess(final long callbackId) {
        requestFileAccessPermissionHelper(result -> {
            DownloadControllerJni.get().onAcquirePermissionResult(
                    callbackId, result.first, result.second);
        });
    }

    /**
     * Requests the stoarge permission from Java.
     * @param callback Callback to notify if the permission is granted or not.
     */
    public static void requestFileAccessPermission(final Callback<Boolean> callback) {
        requestFileAccessPermissionHelper(result -> {
            boolean granted = result.first;
            String permissions = result.second;
            if (granted || permissions == null) {
                callback.onResult(granted);
                return;
            }
            // TODO(jianli): When the permission request was denied by the user and "Never ask
            // again" was checked, we'd better show the permission update infobar to remind the
            // user. Currently the infobar only works for ChromeActivities. We need to investigate
            // how to make it work for other activities.
            callback.onResult(false);
        });
    }

    private static void requestFileAccessPermissionHelper(
            final Callback<Pair<Boolean, String>> callback) {
        AndroidPermissionDelegate delegate = null;
        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        if (activity instanceof ChromeActivity) {
            WindowAndroid windowAndroid = ((ChromeActivity) activity).getWindowAndroid();
            if (windowAndroid != null) {
                delegate = windowAndroid;
            }
        } else if (activity instanceof DownloadActivity) {
            delegate = ((DownloadActivity) activity).getAndroidPermissionDelegate();
        }

        if (delegate == null) {
            callback.onResult(Pair.create(false, null));
            return;
        }

        if (delegate.hasPermission(permission.WRITE_EXTERNAL_STORAGE)) {
            callback.onResult(Pair.create(true, null));
            return;
        }

        if (!delegate.canRequestPermission(permission.WRITE_EXTERNAL_STORAGE)) {
            callback.onResult(Pair.create(false,
                    delegate.isPermissionRevokedByPolicy(permission.WRITE_EXTERNAL_STORAGE)
                            ? null
                            : permission.WRITE_EXTERNAL_STORAGE));
            return;
        }

        final AndroidPermissionDelegate permissionDelegate = delegate;
        final PermissionCallback permissionCallback = (permissions, grantResults)
                -> callback.onResult(Pair.create(grantResults.length > 0
                                && grantResults[0] == PackageManager.PERMISSION_GRANTED,
                        null));

        AndroidPermissionRequester.showMissingPermissionDialog(activity,
                R.string.missing_storage_permission_download_education_text,
                ()
                        -> permissionDelegate.requestPermissions(
                                new String[] {permission.WRITE_EXTERNAL_STORAGE},
                                permissionCallback),
                callback.bind(Pair.create(false, null)));
    }

    /**
     * Enqueue a request to download a file using Android DownloadManager.
     * @param url Url to download.
     * @param userAgent User agent to use.
     * @param contentDisposition Content disposition of the request.
     * @param mimeType MIME type.
     * @param cookie Cookie to use.
     * @param referrer Referrer to use.
     */
    @CalledByNative
    private static void enqueueAndroidDownloadManagerRequest(String url, String userAgent,
            String fileName, String mimeType, String cookie, String referrer) {
        DownloadInfo downloadInfo = new DownloadInfo.Builder()
                .setUrl(url)
                .setUserAgent(userAgent)
                .setFileName(fileName)
                .setMimeType(mimeType)
                .setCookie(cookie)
                .setReferrer(referrer)
                .setIsGETRequest(true)
                .build();
        enqueueDownloadManagerRequest(downloadInfo);
    }

    /**
     * Enqueue a request to download a file using Android DownloadManager.
     *
     * @param info Download information about the download.
     */
    static void enqueueDownloadManagerRequest(final DownloadInfo info) {
        DownloadManagerService.getDownloadManagerService().enqueueNewDownload(
                new DownloadItem(true, info), true);
    }

    /**
     * Called when a download is started.
     */
    @CalledByNative
    private static void onDownloadStarted() {
        if (!BrowserStartupController.getInstance().isFullBrowserStarted()) return;
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.DOWNLOAD_PROGRESS_INFOBAR)) return;
        DownloadUtils.showDownloadStartToast(ContextUtils.getApplicationContext());
    }

    @NativeMethods
    interface Natives {
        void onAcquirePermissionResult(long callbackId, boolean granted, String permissionToUpdate);
    }
}
