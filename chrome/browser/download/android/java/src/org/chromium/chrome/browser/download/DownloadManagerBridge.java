// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.DownloadManager;
import android.content.Context;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;
import android.os.Environment;
import android.text.TextUtils;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.ContentUriUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.components.browser_ui.util.DownloadUtils;
import org.chromium.url.GURL;

import java.io.File;
import java.util.concurrent.RejectedExecutionException;

/** A wrapper for Android DownloadManager to provide utility functions. */
public class DownloadManagerBridge {
    private static final String TAG = "DownloadDelegate";
    private static final String DOWNLOAD_DIRECTORY = "Download";
    private static final String DOWNLOAD_ID_MAPPINGS_FILE_NAME = "download_id_mappings";
    private static final Object sLock = new Object();

    /** Result for querying the Android DownloadManager. */
    public static class DownloadQueryResult {
        public final long downloadId;
        public int downloadStatus;
        public String fileName;
        public String mimeType;
        public Uri contentUri;
        public long lastModifiedTime;
        public long bytesDownloaded;
        public long bytesTotal;
        public int failureReason;
        public String filePath;

        public DownloadQueryResult(long downloadId) {
            this.downloadId = downloadId;
        }
    }

    /**
     * Contains the request params associated with a call to {@link
     * DownloadManagerBridge.enqueueNewDownload}.
     */
    public static class DownloadEnqueueRequest {
        public String url;
        public String fileName;
        public String description;
        public String mimeType;
        public String cookie;
        public String referrer;
        public String userAgent;
        public boolean notifyCompleted;
    }

    /** Contains the results from the call to {@link DownloadManagerBridge.enqueueNewDownload}. */
    public static class DownloadEnqueueResponse {
        public long downloadId = DownloadUtils.INVALID_SYSTEM_DOWNLOAD_ID;
        public boolean result;
        public int failureReason;
        public long startTime;
    }

    /**
     * Adds a download to the Android DownloadManager.
     * @see android.app.DownloadManager#addCompletedDownload(String, String, boolean, String,
     * String, long, boolean)
     */
    public static long addCompletedDownload(
            String fileName,
            String description,
            String mimeType,
            String filePath,
            long fileSizeBytes,
            GURL originalUrl,
            GURL referer,
            String downloadGuid) {
        assert !ThreadUtils.runningOnUiThread();
        assert VERSION.SDK_INT < VERSION_CODES.Q
                : "addCompletedDownload is deprecated in Q, may cause crash.";
        long downloadId = getDownloadIdForDownloadGuid(downloadGuid);
        if (downloadId != DownloadUtils.INVALID_SYSTEM_DOWNLOAD_ID) return downloadId;

        downloadId =
                DownloadUtils.addCompletedDownload(
                        fileName,
                        description,
                        mimeType,
                        filePath,
                        fileSizeBytes,
                        originalUrl,
                        referer);
        if (downloadId != DownloadUtils.INVALID_SYSTEM_DOWNLOAD_ID) {
            addDownloadIdMapping(downloadId, downloadGuid);
        }
        return downloadId;
    }

    /**
     * Removes a download from Android DownloadManager.
     * @param downloadGuid The GUID of the download.
     * @param externallyRemoved If download is externally removed in other application.
     */
    @CalledByNative
    public static void removeCompletedDownload(String downloadGuid, boolean externallyRemoved) {
        PostTask.postTask(
                TaskTraits.BEST_EFFORT_MAY_BLOCK,
                () -> {
                    long downloadId = removeDownloadIdMapping(downloadGuid);

                    // Let Android DownloadManager to remove download only if the user removed the
                    // file in Chrome. If the user renamed or moved the file, Chrome should keep
                    // it intact.
                    if (downloadId != DownloadUtils.INVALID_SYSTEM_DOWNLOAD_ID
                            && !externallyRemoved) {
                        DownloadManager manager =
                                (DownloadManager)
                                        getContext().getSystemService(Context.DOWNLOAD_SERVICE);
                        manager.remove(downloadId);
                    }
                });
    }

    /**
     * Sends the download request to Android download manager. If |notifyCompleted| is true,
     * a notification will be sent to the user once download is complete and the downloaded
     * content will be saved to the public directory on external storage. Otherwise, the
     * download will be saved in the app directory and user will not get any notifications
     * after download completion.
     * This will be used by OMA downloads as we need Android DownloadManager to encrypt the content.
     *
     * @param request The download request params.
     * @param callback The callback to be executed after the download request is enqueued.
     */
    public static void enqueueNewDownload(
            DownloadEnqueueRequest request, Callback<DownloadEnqueueResponse> callback) {
        new EnqueueNewDownloadTask(request, callback)
                .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Query the Android DownloadManager for download status.
     * @param downloadId The id of the download.
     * @param callback Callback to be notified when query completes.
     */
    public static void queryDownloadResult(
            long downloadId, Callback<DownloadQueryResult> callback) {
        DownloadQueryTask task = new DownloadQueryTask(downloadId, callback);
        task.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Query the Android DownloadManager for download status.
     * @param downloadId The id of the download.
     */
    public static DownloadQueryResult queryDownloadResult(long downloadId) {
        assert !ThreadUtils.runningOnUiThread();
        DownloadQueryResult result = new DownloadQueryResult(downloadId);
        DownloadManager manager =
                (DownloadManager) getContext().getSystemService(Context.DOWNLOAD_SERVICE);
        try {
            Cursor c = manager.query(new DownloadManager.Query().setFilterById(downloadId));
            if (c == null) {
                result.downloadStatus = DownloadStatus.CANCELLED;
                return result;
            }
            result.downloadStatus = DownloadStatus.IN_PROGRESS;
            if (c.moveToNext()) {
                int status = c.getInt(c.getColumnIndexOrThrow(DownloadManager.COLUMN_STATUS));
                result.downloadStatus = getDownloadStatus(status);
                result.fileName =
                        c.getString(c.getColumnIndexOrThrow(DownloadManager.COLUMN_TITLE));
                result.failureReason =
                        c.getInt(c.getColumnIndexOrThrow(DownloadManager.COLUMN_REASON));
                result.lastModifiedTime =
                        c.getLong(
                                c.getColumnIndexOrThrow(
                                        DownloadManager.COLUMN_LAST_MODIFIED_TIMESTAMP));
                result.bytesDownloaded =
                        c.getLong(
                                c.getColumnIndexOrThrow(
                                        DownloadManager.COLUMN_BYTES_DOWNLOADED_SO_FAR));
                result.bytesTotal =
                        c.getLong(c.getColumnIndexOrThrow(DownloadManager.COLUMN_TOTAL_SIZE_BYTES));
                String localUri =
                        c.getString(c.getColumnIndexOrThrow(DownloadManager.COLUMN_LOCAL_URI));
                if (!TextUtils.isEmpty(localUri)) {
                    Uri uri = Uri.parse(localUri);
                    result.filePath = uri.getPath();
                }
            } else {
                result.downloadStatus = DownloadStatus.CANCELLED;
            }
            c.close();

            try {
                result.contentUri = manager.getUriForDownloadedFile(downloadId);
            } catch (SecurityException e) {
                Log.e(TAG, "unable to get content URI from DownloadManager");
            }

            result.mimeType = manager.getMimeTypeForDownloadedFile(downloadId);
        } catch (Exception e) {
            result.downloadStatus = DownloadStatus.CANCELLED;
            Log.e(TAG, "unable to query android DownloadManager", e);
        }

        return result;
    }

    /**
     * @return The android DownloadManager's download ID for the given download.
     */
    public static long getDownloadIdForDownloadGuid(String downloadGuid) {
        return getSharedPreferences()
                .getLong(downloadGuid, DownloadUtils.INVALID_SYSTEM_DOWNLOAD_ID);
    }

    /**
     * Inserts a new download ID mapping into the SharedPreferences
     *
     * @param downloadId system download ID from Android DownloadManager.
     * @param downloadGuid Download GUID.
     */
    private static void addDownloadIdMapping(long downloadId, String downloadGuid) {
        synchronized (sLock) {
            SharedPreferences sharedPrefs = getSharedPreferences();
            SharedPreferences.Editor editor = sharedPrefs.edit();
            editor.putLong(downloadGuid, downloadId);
            editor.apply();
        }
    }

    /**
     * Removes a download Id mapping from the SharedPreferences given the download GUID.
     * @param downloadGuid Download GUID.
     * @return the Android DownloadManager's download ID that is removed, or
     *         INVALID_SYSTEM_DOWNLOAD_ID if it is not found.
     */
    private static long removeDownloadIdMapping(String downloadGuid) {
        long downloadId = DownloadUtils.INVALID_SYSTEM_DOWNLOAD_ID;
        synchronized (sLock) {
            SharedPreferences sharedPrefs = getSharedPreferences();
            downloadId =
                    sharedPrefs.getLong(downloadGuid, DownloadUtils.INVALID_SYSTEM_DOWNLOAD_ID);
            if (downloadId != DownloadUtils.INVALID_SYSTEM_DOWNLOAD_ID) {
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
    private static SharedPreferences getSharedPreferences() {
        return ContextUtils.getApplicationContext()
                .getSharedPreferences(DOWNLOAD_ID_MAPPINGS_FILE_NAME, Context.MODE_PRIVATE);
    }

    private static Context getContext() {
        return ContextUtils.getApplicationContext();
    }

    /**
     * This function is meant to be called as the last step of a download. It will add the download
     * to the android's DownloadManager if the download is not a content URI.
     */
    @CalledByNative
    private static void addCompletedDownload(
            String fileName,
            String description,
            String originalMimeType,
            String filePath,
            long fileSizeBytes,
            GURL originalUrl,
            GURL referrer,
            String downloadGuid,
            long callbackId) {
        final String mimeType =
                MimeUtils.remapGenericMimeType(originalMimeType, originalUrl.getSpec(), fileName);
        AsyncTask<Long> task =
                new AsyncTask<Long>() {
                    @Override
                    protected Long doInBackground() {
                        long downloadId = DownloadConstants.INVALID_DOWNLOAD_ID;
                        // On Android Q-, add the completed download to Android download manager.
                        if (!ContentUriUtils.isContentUri(filePath)
                                && Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
                            downloadId =
                                    addCompletedDownload(
                                            fileName,
                                            description,
                                            mimeType,
                                            filePath,
                                            fileSizeBytes,
                                            originalUrl,
                                            referrer,
                                            downloadGuid);
                        }
                        return downloadId;
                    }

                    @Override
                    protected void onPostExecute(Long downloadId) {
                        DownloadManagerBridgeJni.get()
                                .onAddCompletedDownloadDone(callbackId, downloadId);
                    }
                };
        try {
            task.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        } catch (RejectedExecutionException e) {
            // Reaching thread limit, update will be reschduled for the next run.
            Log.e(TAG, "Thread limit reached, reschedule notification update later.");
            DownloadManagerBridgeJni.get()
                    .onAddCompletedDownloadDone(callbackId, DownloadConstants.INVALID_DOWNLOAD_ID);
        }
    }

    private static int getDownloadStatus(int downloadManagerStatus) {
        switch (downloadManagerStatus) {
            case DownloadManager.STATUS_SUCCESSFUL:
                return DownloadStatus.COMPLETE;
            case DownloadManager.STATUS_FAILED:
                return DownloadStatus.FAILED;
            default:
                return DownloadStatus.IN_PROGRESS;
        }
    }

    /** Async task to query download status from Android DownloadManager */
    private static class DownloadQueryTask extends AsyncTask<DownloadQueryResult> {
        private final long mDownloadId;
        private final Callback<DownloadQueryResult> mCallback;

        public DownloadQueryTask(long downloadId, Callback<DownloadQueryResult> callback) {
            mDownloadId = downloadId;
            mCallback = callback;
        }

        @Override
        public DownloadQueryResult doInBackground() {
            return queryDownloadResult(mDownloadId);
        }

        @Override
        protected void onPostExecute(DownloadQueryResult result) {
            mCallback.onResult(result);
        }
    }

    /** Async task to enqueue a download request into DownloadManager. */
    private static class EnqueueNewDownloadTask extends AsyncTask<Boolean> {
        private final DownloadEnqueueRequest mEnqueueRequest;
        private final Callback<DownloadEnqueueResponse> mCallback;
        private long mDownloadId;
        private int mFailureReason;
        private long mStartTime;

        public EnqueueNewDownloadTask(
                DownloadEnqueueRequest enqueueRequest, Callback<DownloadEnqueueResponse> callback) {
            mEnqueueRequest = enqueueRequest;
            mCallback = callback;
        }

        @Override
        public Boolean doInBackground() {
            DownloadManager.Request request;
            try {
                request = new DownloadManager.Request(Uri.parse(mEnqueueRequest.url));
            } catch (IllegalArgumentException e) {
                Log.e(TAG, "Cannot download non http or https scheme");
                // Use ERROR_UNHANDLED_HTTP_CODE so that it will be treated as a server error.
                mFailureReason = DownloadManager.ERROR_UNHANDLED_HTTP_CODE;
                return false;
            }

            request.setMimeType(mEnqueueRequest.mimeType);
            try {
                if (mEnqueueRequest.notifyCompleted) {
                    if (mEnqueueRequest.fileName != null) {
                        // Set downloaded file destination to /sdcard/Download or, should it be
                        // set to one of several Environment.DIRECTORY* dirs depending on mimetype?
                        request.setDestinationInExternalPublicDir(
                                Environment.DIRECTORY_DOWNLOADS, mEnqueueRequest.fileName);
                    }
                } else {
                    File dir = new File(getContext().getExternalFilesDir(null), DOWNLOAD_DIRECTORY);
                    if (dir.mkdir() || dir.isDirectory()) {
                        File file = new File(dir, mEnqueueRequest.fileName);
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

            if (mEnqueueRequest.notifyCompleted) {
                // Let this downloaded file be scanned by MediaScanner - so that it can
                // show up in Gallery app, for example.
                request.allowScanningByMediaScanner();
                request.setNotificationVisibility(
                        DownloadManager.Request.VISIBILITY_VISIBLE_NOTIFY_COMPLETED);
            } else {
                request.setNotificationVisibility(DownloadManager.Request.VISIBILITY_VISIBLE);
            }
            String description = mEnqueueRequest.description;
            if (TextUtils.isEmpty(description)) {
                description = mEnqueueRequest.fileName;
            }
            request.setDescription(description);
            request.setTitle(mEnqueueRequest.fileName);
            request.addRequestHeader("Cookie", mEnqueueRequest.cookie);
            request.addRequestHeader("referrer", mEnqueueRequest.referrer);
            request.addRequestHeader("User-Agent", mEnqueueRequest.userAgent);

            DownloadManager manager =
                    (DownloadManager) getContext().getSystemService(Context.DOWNLOAD_SERVICE);
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
            DownloadEnqueueResponse enqueueResult = new DownloadEnqueueResponse();
            enqueueResult.result = result;
            enqueueResult.failureReason = mFailureReason;
            enqueueResult.downloadId = mDownloadId;
            enqueueResult.startTime = mStartTime;
            mCallback.onResult(enqueueResult);
        }
    }

    @NativeMethods
    interface Natives {
        void onAddCompletedDownloadDone(long callbackId, long downloadId);
    }
}
