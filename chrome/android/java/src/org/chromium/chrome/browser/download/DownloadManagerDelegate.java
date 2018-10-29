// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.DownloadManager;
import android.content.Context;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.support.v4.app.NotificationManagerCompat;
import android.text.TextUtils;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.StrictModeContext;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.browser.UrlConstants;

import java.io.File;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

/**
 * A wrapper for Android DownloadManager to provide utility functions.
 */
public class DownloadManagerDelegate {
    private static final String TAG = "DownloadDelegate";
    private static final String DOWNLOAD_DIRECTORY = "Download";
    private static final long INVALID_SYSTEM_DOWNLOAD_ID = -1;
    private static final String DOWNLOAD_ID_MAPPINGS_FILE_NAME = "download_id_mappings";
    private static final Object sLock = new Object();
    protected final Context mContext;

    public DownloadManagerDelegate(Context context) {
        mContext = context;
    }

    /**
     * Inserts a new download ID mapping into the SharedPreferences
     * @param downloadId system download ID from Android DownloadManager.
     * @param downloadGuid Download GUID.
     */
    private void addDownloadIdMapping(long downloadId, String downloadGuid) {
        synchronized (sLock) {
            SharedPreferences sharedPrefs = getSharedPreferences();
            SharedPreferences.Editor editor = sharedPrefs.edit();
            editor.putLong(downloadGuid, downloadId);
            editor.apply();
        }
    }

    /**
     * Removes a download Id mapping from the SharedPreferences given the download GUID.
     * @param guid Download GUID.
     * @return the Android DownloadManager's download ID that is removed, or
     *         INVALID_SYSTEM_DOWNLOAD_ID if it is not found.
     */
    private long removeDownloadIdMapping(String downloadGuid) {
        long downloadId = INVALID_SYSTEM_DOWNLOAD_ID;
        synchronized (sLock) {
            SharedPreferences sharedPrefs = getSharedPreferences();
            downloadId = sharedPrefs.getLong(downloadGuid, INVALID_SYSTEM_DOWNLOAD_ID);
            if (downloadId != INVALID_SYSTEM_DOWNLOAD_ID) {
                SharedPreferences.Editor editor = sharedPrefs.edit();
                editor.remove(downloadGuid);
                editor.apply();
            }
        }
        return downloadId;
    }

    /**
     * Lazily retrieve the SharedPreferences when needed. Since download operations are not very
     * frequent, no need to load all SharedPreference entries into a hashmap in the memory.
     * @return the SharedPreferences instance.
     */
    private SharedPreferences getSharedPreferences() {
        return ContextUtils.getApplicationContext().getSharedPreferences(
                DOWNLOAD_ID_MAPPINGS_FILE_NAME, Context.MODE_PRIVATE);
    }

    /**
     * @see android.app.DownloadManager#addCompletedDownload(String, String, boolean, String,
     * String, long, boolean)
     */
    protected long addCompletedDownload(String fileName, String description, String mimeType,
            String path, long length, String originalUrl, String referer, String downloadGuid) {
        DownloadManager manager =
                (DownloadManager) mContext.getSystemService(Context.DOWNLOAD_SERVICE);
        NotificationManagerCompat notificationManager = NotificationManagerCompat.from(mContext);
        boolean useSystemNotification = !notificationManager.areNotificationsEnabled();
        long downloadId = -1;
        if (Build.VERSION.SDK_INT > Build.VERSION_CODES.M) {
            Class<?> c = manager.getClass();
            try {
                Class[] args = {String.class, String.class, boolean.class, String.class,
                        String.class, long.class, boolean.class, Uri.class, Uri.class};
                Method method = c.getMethod("addCompletedDownload", args);
                // OriginalUri has to be null or non-empty http(s) scheme.
                Uri originalUri = TextUtils.isEmpty(originalUrl) ? null : Uri.parse(originalUrl);
                if (originalUri != null) {
                    String scheme = originalUri.normalizeScheme().getScheme();
                    if (scheme == null || (!scheme.equals(UrlConstants.HTTPS_SCHEME)
                            && !scheme.equals(UrlConstants.HTTP_SCHEME))) {
                        originalUri = null;
                    }
                }
                Uri refererUri = TextUtils.isEmpty(referer) ? null : Uri.parse(referer);
                downloadId = (Long) method.invoke(manager, fileName, description, true, mimeType,
                        path, length, useSystemNotification, originalUri, refererUri);
            } catch (SecurityException e) {
                Log.e(TAG, "Cannot access the needed method.");
            } catch (NoSuchMethodException e) {
                Log.e(TAG, "Cannot find the needed method.");
            } catch (InvocationTargetException e) {
                Log.e(TAG, "Error calling the needed method.");
            } catch (IllegalAccessException e) {
                Log.e(TAG, "Error accessing the needed method.");
            }
        } else {
            downloadId = manager.addCompletedDownload(fileName, description, true, mimeType, path,
                    length, useSystemNotification);
        }
        addDownloadIdMapping(downloadId, downloadGuid);
        return downloadId;
    }

    /**
     * Removes a download from Android DownloadManager.
     * @param downloadGuid The GUID of the download.
     * @param externallyRemoved If download is externally removed in other application.
     */
    void removeCompletedDownload(String downloadGuid, boolean externallyRemoved) {
        long downloadId = removeDownloadIdMapping(downloadGuid);

        // Let Android DownloadManager to remove download only if the user removed the file in
        // Chrome. If the user renamed or moved the file, Chrome should keep it intact.
        if (downloadId != INVALID_SYSTEM_DOWNLOAD_ID && !externallyRemoved) {
            DownloadManager manager =
                    (DownloadManager) mContext.getSystemService(Context.DOWNLOAD_SERVICE);
            manager.remove(downloadId);
        }
    }

    /**
     * Interface for returning the query result when it completes.
     */
    public interface DownloadQueryCallback {
        /**
         * Callback function to return query result.
         * @param result Query result from android DownloadManager.
         * @param showNotifications Whether to show status notifications.
         */
        public void onQueryCompleted(DownloadQueryResult result, boolean showNotifications);
    }

    /**
     * Result for querying the Android DownloadManager.
     */
    static class DownloadQueryResult {
        public final DownloadItem item;
        public final int downloadStatus;
        public final long downloadTimeInMilliseconds;
        public final long bytesDownloaded;
        public final boolean canResolve;
        public final int failureReason;

        DownloadQueryResult(DownloadItem item, int downloadStatus, long downloadTimeInMilliseconds,
                long bytesDownloaded, boolean canResolve, int failureReason) {
            this.item = item;
            this.downloadStatus = downloadStatus;
            this.downloadTimeInMilliseconds = downloadTimeInMilliseconds;
            this.canResolve = canResolve;
            this.bytesDownloaded = bytesDownloaded;
            this.failureReason = failureReason;
        }
    }

    /**
     * Query the Android DownloadManager for download status.
     * @param downloadItem Download item to query.
     * @param showNotifications Whether to show status notifications.
     * @param callback Callback to be notified when query completes.
     */
    void queryDownloadResult(
            DownloadItem downloadItem, boolean showNotifications, DownloadQueryCallback callback) {
        DownloadQueryTask task = new DownloadQueryTask(downloadItem, showNotifications, callback);
        task.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Async task to query download status from Android DownloadManager
     */
    private class DownloadQueryTask extends AsyncTask<DownloadQueryResult> {
        private final DownloadItem mDownloadItem;
        private final boolean mShowNotifications;
        private final DownloadQueryCallback mCallback;

        public DownloadQueryTask(DownloadItem downloadItem, boolean showNotifications,
                DownloadQueryCallback callback) {
            mDownloadItem = downloadItem;
            mShowNotifications = showNotifications;
            mCallback = callback;
        }

        @Override
        public DownloadQueryResult doInBackground() {
            DownloadManager manager =
                    (DownloadManager) mContext.getSystemService(Context.DOWNLOAD_SERVICE);
            Cursor c = manager.query(
                    new DownloadManager.Query().setFilterById(mDownloadItem.getSystemDownloadId()));
            if (c == null) {
                return new DownloadQueryResult(mDownloadItem,
                        DownloadManagerService.DownloadStatus.CANCELLED, 0, 0, false, 0);
            }
            long bytesDownloaded = 0;
            boolean canResolve = false;
            @DownloadManagerService.DownloadStatus
            int downloadStatus = DownloadManagerService.DownloadStatus.IN_PROGRESS;
            int failureReason = 0;
            long lastModifiedTime = 0;
            if (c.moveToNext()) {
                int statusIndex = c.getColumnIndex(DownloadManager.COLUMN_STATUS);
                int status = c.getInt(c.getColumnIndex(DownloadManager.COLUMN_STATUS));
                if (status == DownloadManager.STATUS_SUCCESSFUL) {
                    downloadStatus = DownloadManagerService.DownloadStatus.COMPLETE;
                    DownloadInfo.Builder builder = mDownloadItem.getDownloadInfo() == null
                            ? new DownloadInfo.Builder()
                            : DownloadInfo.Builder.fromDownloadInfo(
                                      mDownloadItem.getDownloadInfo());
                    builder.setFileName(
                            c.getString(c.getColumnIndex(DownloadManager.COLUMN_TITLE)));
                    mDownloadItem.setDownloadInfo(builder.build());
                    if (mShowNotifications) {
                        canResolve = DownloadManagerService.isOMADownloadDescription(
                                mDownloadItem.getDownloadInfo())
                                || DownloadManagerService.canResolveDownloadItem(
                                        mContext, mDownloadItem, false);
                    }
                } else if (status == DownloadManager.STATUS_FAILED) {
                    downloadStatus = DownloadManagerService.DownloadStatus.FAILED;
                    failureReason = c.getInt(c.getColumnIndex(DownloadManager.COLUMN_REASON));
                }
                lastModifiedTime =
                        c.getLong(c.getColumnIndex(DownloadManager.COLUMN_LAST_MODIFIED_TIMESTAMP));
                bytesDownloaded =
                        c.getLong(c.getColumnIndex(DownloadManager.COLUMN_BYTES_DOWNLOADED_SO_FAR));
            } else {
                downloadStatus = DownloadManagerService.DownloadStatus.CANCELLED;
            }
            c.close();
            long totalTime = Math.max(0, lastModifiedTime - mDownloadItem.getStartTime());
            return new DownloadQueryResult(mDownloadItem, downloadStatus, totalTime,
                    bytesDownloaded, canResolve, failureReason);
        }

        @Override
        protected void onPostExecute(DownloadQueryResult result) {
            mCallback.onQueryCompleted(result, mShowNotifications);
        }
    }

    static Uri getContentUriFromDownloadManager(Context context, long downloadId) {
        DownloadManager manager =
                (DownloadManager) context.getSystemService(Context.DOWNLOAD_SERVICE);
        Uri contentUri = null;
        try {
            try (StrictModeContext unused = StrictModeContext.allowDiskReads()) {
                contentUri = manager.getUriForDownloadedFile(downloadId);
            }
        } catch (SecurityException e) {
            Log.e(TAG, "unable to get content URI from DownloadManager");
        }
        return contentUri;
    }

    /**
     * Sends the download request to Android download manager. If |notifyCompleted| is true,
     * a notification will be sent to the user once download is complete and the downloaded
     * content will be saved to the public directory on external storage. Otherwise, the
     * download will be saved in the app directory and user will not get any notifications
     * after download completion.
     * This will be used by OMA downloads as we need Android DownloadManager to encrypt the content.
     *
     * @param item The associated {@link DownloadItem}.
     * @param notifyCompleted Whether to notify the user after DownloadManager completes the
     *                        download.
     * @param callback The callback to be executed after the download request is enqueued.
     */
    public void enqueueDownloadManagerRequest(final DownloadItem item, boolean notifyCompleted,
            EnqueueDownloadRequestCallback callback) {
        new EnqueueDownloadRequestTask(item, notifyCompleted, callback)
                .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Interface for returning the enqueue result when it completes.
     */
    public interface EnqueueDownloadRequestCallback {
        /**
         * Callback function to return result of enqueue download operation.
         * @param result Result of enqueue operation on android DownloadManager.
         * @param failureReason The reason for failure if any, provided by android DownloadManager.
         * @param downloadItem The associated download item.
         * @param downloadId The download id obtained from android DownloadManager as a result of
         * this enqueue operation.
         */
        public void onDownloadEnqueued(
                boolean result, int failureReason, DownloadItem downloadItem, long downloadId);
    }

    /**
     * Async task to enqueue a download request into DownloadManager.
     */
    private class EnqueueDownloadRequestTask extends AsyncTask<Boolean> {
        private final DownloadItem mDownloadItem;
        private final boolean mNotifyCompleted;
        private final EnqueueDownloadRequestCallback mCallback;
        private long mDownloadId;
        private int mFailureReason;
        private long mStartTime;

        public EnqueueDownloadRequestTask(DownloadItem downloadItem, Boolean notifyCompleted,
                EnqueueDownloadRequestCallback callback) {
            mDownloadItem = downloadItem;
            mNotifyCompleted = notifyCompleted;
            mCallback = callback;
        }

        @Override
        public Boolean doInBackground() {
            Uri uri = Uri.parse(mDownloadItem.getDownloadInfo().getUrl());
            DownloadManager.Request request;
            DownloadInfo info = mDownloadItem.getDownloadInfo();
            try {
                request = new DownloadManager.Request(uri);
            } catch (IllegalArgumentException e) {
                Log.e(TAG, "Cannot download non http or https scheme");
                // Use ERROR_UNHANDLED_HTTP_CODE so that it will be treated as a server error.
                mFailureReason = DownloadManager.ERROR_UNHANDLED_HTTP_CODE;
                return false;
            }

            request.setMimeType(info.getMimeType());
            try {
                if (mNotifyCompleted) {
                    if (info.getFileName() != null) {
                        // Set downloaded file destination to /sdcard/Download or, should it be
                        // set to one of several Environment.DIRECTORY* dirs depending on mimetype?
                        request.setDestinationInExternalPublicDir(
                                Environment.DIRECTORY_DOWNLOADS, info.getFileName());
                    }
                } else {
                    File dir = new File(mContext.getExternalFilesDir(null), DOWNLOAD_DIRECTORY);
                    if (dir.mkdir() || dir.isDirectory()) {
                        File file = new File(dir, info.getFileName());
                        request.setDestinationUri(Uri.fromFile(file));
                    } else {
                        Log.e(TAG, "Cannot create download directory");
                        mFailureReason = DownloadManager.ERROR_FILE_ERROR;
                        return false;
                    }
                }
            } catch (IllegalStateException e) {
                Log.e(TAG, "Cannot create download directory");
                mFailureReason = DownloadManager.ERROR_FILE_ERROR;
                return false;
            }

            if (mNotifyCompleted) {
                // Let this downloaded file be scanned by MediaScanner - so that it can
                // show up in Gallery app, for example.
                request.allowScanningByMediaScanner();
                request.setNotificationVisibility(
                        DownloadManager.Request.VISIBILITY_VISIBLE_NOTIFY_COMPLETED);
            } else {
                request.setNotificationVisibility(DownloadManager.Request.VISIBILITY_VISIBLE);
            }
            String description = info.getDescription();
            if (TextUtils.isEmpty(description)) {
                description = info.getFileName();
            }
            request.setDescription(description);
            request.setTitle(info.getFileName());
            request.addRequestHeader("Cookie", info.getCookie());
            request.addRequestHeader("Referer", info.getReferrer());
            request.addRequestHeader("User-Agent", info.getUserAgent());

            DownloadManager manager =
                    (DownloadManager) mContext.getSystemService(Context.DOWNLOAD_SERVICE);
            try {
                mStartTime = System.currentTimeMillis();
                mDownloadId = manager.enqueue(request);
            } catch (IllegalArgumentException e) {
                // See crbug.com/143499 for more details.
                Log.e(TAG, "Download failed: " + e);
                mFailureReason = DownloadManager.ERROR_UNKNOWN;
                return false;
            } catch (RuntimeException e) {
                // See crbug.com/490442 for more details.
                Log.e(TAG, "Failed to create target file on the external storage: " + e);
                mFailureReason = DownloadManager.ERROR_FILE_ERROR;
                return false;
            }
            return true;
        }

        @Override
        protected void onPostExecute(Boolean result) {
            mDownloadItem.setStartTime(mStartTime);
            mCallback.onDownloadEnqueued(result, mFailureReason, mDownloadItem, mDownloadId);
            mDownloadItem.setSystemDownloadId(mDownloadId);
        }
    }
}
