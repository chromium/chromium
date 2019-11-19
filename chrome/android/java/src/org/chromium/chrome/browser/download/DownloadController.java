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
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.browser.permissions.AndroidPermissionRequester;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.download.DownloadCollectionBridge;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.AndroidPermissionDelegate;
import org.chromium.ui.base.PermissionCallback;
import org.chromium.ui.base.WindowAndroid;

/**
 * Java counterpart of android DownloadController.
 *
 * Its a singleton class instantiated by the C++ DownloadController.
 */
public class DownloadController {
    /**
     * Class for notifying the application that download has completed.
     */
    public interface DownloadNotificationService {
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

    private static DownloadNotificationService sDownloadNotificationService;

    public static void setDownloadNotificationService(DownloadNotificationService service) {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.DOWNLOAD_OFFLINE_CONTENT_PROVIDER)) {
            return;
        }

        sDownloadNotificationService = service;
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

        if (sDownloadNotificationService == null) return;
        sDownloadNotificationService.onDownloadCompleted(downloadInfo);
    }

    /**
     * Notifies the download delegate that a download completed and passes along info about the
     * download. This can be either a POST download or a GET download with authentication.
     */
    @CalledByNative
    private static void onDownloadInterrupted(DownloadInfo downloadInfo, boolean isAutoResumable) {
        if (sDownloadNotificationService == null) return;
        sDownloadNotificationService.onDownloadInterrupted(downloadInfo, isAutoResumable);
    }

    /**
     * Called when a download was cancelled.
     */
    @CalledByNative
    private static void onDownloadCancelled(DownloadInfo downloadInfo) {
        if (sDownloadNotificationService == null) return;
        sDownloadNotificationService.onDownloadCancelled(downloadInfo);
    }

    /**
     * Notifies the download delegate about progress of a download. Downloads that use Chrome
     * network stack use custom notification to display the progress of downloads.
     */
    @CalledByNative
    private static void onDownloadUpdated(DownloadInfo downloadInfo) {
        if (sDownloadNotificationService == null) return;
        sDownloadNotificationService.onDownloadUpdated(downloadInfo);
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
                () -> permissionDelegate.requestPermissions(
                                new String[] {permission.WRITE_EXTERNAL_STORAGE},
                                permissionCallback),
                () -> callback.onResult(Pair.create(false, null)));
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
        if (!BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER)
                        .isFullBrowserStarted()) {
            return;
        }
        if (FeatureUtilities.isDownloadProgressInfoBarEnabled()) return;
        DownloadUtils.showDownloadStartToast(ContextUtils.getApplicationContext());
    }

    /**
     * Close a tab if it is blank. Returns true if it is or already closed.
     * @param Tab Tab to close.
     * @return true iff the tab was (already) closed.
     */
    @CalledByNative
    static boolean closeTabIfBlank(Tab tab) {
        if (tab == null) return true;
        WebContents contents = tab.getWebContents();
        boolean isInitialNavigation = contents == null
                || contents.getNavigationController().isInitialNavigation();
        if (isInitialNavigation) {
            // Tab is created just for download, close it.
            TabModelSelector selector = TabModelSelector.from(tab);
            if (selector == null) return true;
            if (selector.getModel(tab.isIncognito()).getCount() == 1) return false;
            boolean closed = selector.closeTab(tab);
            assert closed;
            return true;
        }
        return false;
    }

    @NativeMethods
    interface Natives {
        void onAcquirePermissionResult(long callbackId, boolean granted, String permissionToUpdate);
    }
}
