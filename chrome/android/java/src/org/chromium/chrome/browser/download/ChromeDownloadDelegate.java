// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.Manifest.permission;
import android.app.DownloadManager;
import android.content.pm.PackageManager;
import android.os.Environment;
import android.text.TextUtils;
import android.util.Pair;
import android.webkit.URLUtil;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.UserData;
import org.chromium.base.UserDataHost;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.permissions.PermissionCallback;
import org.chromium.url.GURL;

import java.io.File;

/**
 * Chrome implementation of the ContentViewDownloadDelegate interface.
 *
 * Listens to POST and GET download events. GET download requests are passed along to the
 * Android Download Manager. POST downloads are expected to be handled natively and listener
 * is responsible for adding the completed download to the download manager.
 *
 * Prompts the user when a dangerous file is downloaded. Auto-opens PDFs after downloading.
 */
public class ChromeDownloadDelegate implements UserData {
    private static final String TAG = "Download";

    private static final Class<ChromeDownloadDelegate> USER_DATA_KEY = ChromeDownloadDelegate.class;
    private Tab mTab;

    public static ChromeDownloadDelegate from(Tab tab) {
        UserDataHost host = tab.getUserDataHost();
        ChromeDownloadDelegate controller = host.getUserData(USER_DATA_KEY);
        return controller == null
                ? host.setUserData(USER_DATA_KEY, new ChromeDownloadDelegate(tab))
                : controller;
    }

    /**
     * Creates ChromeDownloadDelegate.
     * @param tab The corresponding tab instance.
     */
    @VisibleForTesting
    ChromeDownloadDelegate(Tab tab) {
        mTab = tab;
    }

    @Override
    public void destroy() {
        mTab = null;
    }

    /**
     * Notify the host application a download should be done, even if there is a
     * streaming viewer available for this type.
     *
     * @param downloadInfo Information about the download.
     */
    protected void onDownloadStartNoStream(final DownloadInfo downloadInfo) {
        final String fileName = downloadInfo.getFileName();
        assert !TextUtils.isEmpty(fileName);
        final String newMimeType =
                MimeUtils.remapGenericMimeType(
                        downloadInfo.getMimeType(), downloadInfo.getUrl().getSpec(), fileName);
        new AsyncTask<Pair<String, File>>() {
            @Override
            protected Pair<String, File> doInBackground() {
                // Check to see if we have an SDCard.
                String status = Environment.getExternalStorageState();
                File fullDirPath = getDownloadDirectoryFullPath();
                return new Pair<String, File>(status, fullDirPath);
            }

            @Override
            protected void onPostExecute(Pair<String, File> result) {
                String externalStorageState = result.first;
                File fullDirPath = result.second;
                if (!checkExternalStorageAndNotify(
                        downloadInfo, fullDirPath, externalStorageState)) {
                    return;
                }
                GURL url = sanitizeDownloadUrl(downloadInfo);
                if (url == null) return;
                DownloadInfo newInfo =
                        DownloadInfo.Builder.fromDownloadInfo(downloadInfo)
                                .setUrl(url)
                                .setMimeType(newMimeType)
                                .setDescription(url.getSpec())
                                .setFileName(fileName)
                                .setIsGETRequest(true)
                                .build();
                DownloadController.enqueueDownloadManagerRequest(newInfo);
                // TODO(shaktisahu): Verify if we still need to close an empty tab for OMA download.
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Sanitize the URL for the download item.
     *
     * @param downloadInfo Information about the download.
     */
    protected GURL sanitizeDownloadUrl(DownloadInfo downloadInfo) {
        return downloadInfo.getUrl();
    }

    /**
     * Return the full path of the download directory.
     *
     * @return File object containing the path to the download directory.
     */
    private static File getDownloadDirectoryFullPath() {
        assert !ThreadUtils.runningOnUiThread();
        File dir = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS);
        if (!dir.mkdir() && !dir.isDirectory()) return null;
        return dir;
    }

    private static boolean checkFileExists(File dirPath, final String fileName) {
        assert !ThreadUtils.runningOnUiThread();
        final File file = new File(dirPath, fileName);
        return file != null && file.exists();
    }

    private static void deleteFileForOverwrite(DownloadInfo info) {
        assert !ThreadUtils.runningOnUiThread();
        File dir = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS);
        if (!dir.isDirectory()) return;
        final File file = new File(dir, info.getFileName());
        if (!file.delete()) {
            Log.e(TAG, "Failed to delete a file: " + info.getFileName());
        }
    }

    /**
     * Check the external storage and notify user on error.
     *
     * @param fullDirPath The dir path to download a file. Normally this is external storage.
     * @param externalStorageStatus The status of the external storage.
     * @return Whether external storage is ok for downloading.
     */
    private boolean checkExternalStorageAndNotify(
            DownloadInfo downloadInfo, File fullDirPath, String externalStorageStatus) {
        if (fullDirPath == null) {
            Log.e(TAG, "Download failed: no SD card");
            alertDownloadFailure(downloadInfo, DownloadManager.ERROR_DEVICE_NOT_FOUND);
            return false;
        }
        if (!externalStorageStatus.equals(Environment.MEDIA_MOUNTED)) {
            int reason = DownloadManager.ERROR_DEVICE_NOT_FOUND;
            // Check to see if the SDCard is busy, same as the music app
            if (externalStorageStatus.equals(Environment.MEDIA_SHARED)) {
                Log.e(TAG, "Download failed: SD card unavailable");
                reason = DownloadManager.ERROR_FILE_ERROR;
            } else {
                Log.e(TAG, "Download failed: no SD card");
            }
            alertDownloadFailure(downloadInfo, reason);
            return false;
        }
        return true;
    }

    /**
     * Alerts user of download failure.
     *
     * @param downloadInfo The associated download.
     * @param reason Reason of failure defined in {@link DownloadManager}
     */
    private void alertDownloadFailure(DownloadInfo downloadInfo, int reason) {
        DownloadItem downloadItem = new DownloadItem(false, downloadInfo);
        DownloadManagerService.getDownloadManagerService().onDownloadFailed(downloadItem, reason);
    }

    /**
     * For certain download types(OMA for example), android DownloadManager should
     * handle them. Call this function to intercept those downloads.
     *
     * @param url URL to be downloaded.
     * @return whether the DownloadManager should intercept the download.
     */
    public boolean shouldInterceptContextMenuDownload(GURL url) {
        if (!UrlUtilities.isHttpOrHttps(url)) return false;
        String path = url.getPath();
        if (!OMADownloadHandler.isOMAFile(path)) return false;
        if (mTab == null) return true;
        String fileName =
                URLUtil.guessFileName(url.getSpec(), null, MimeUtils.OMA_DRM_MESSAGE_MIME);
        final DownloadInfo downloadInfo =
                new DownloadInfo.Builder().setUrl(url).setFileName(fileName).build();
        WindowAndroid window = mTab.getWindowAndroid();
        if (window.hasPermission(permission.WRITE_EXTERNAL_STORAGE)) {
            onDownloadStartNoStream(downloadInfo);
        } else if (window.canRequestPermission(permission.WRITE_EXTERNAL_STORAGE)) {
            PermissionCallback permissionCallback =
                    (permissions, grantResults) -> {
                        if (grantResults.length > 0
                                && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                            onDownloadStartNoStream(downloadInfo);
                        }
                    };
            window.requestPermissions(
                    new String[] {permission.WRITE_EXTERNAL_STORAGE}, permissionCallback);
        }
        return true;
    }
}
