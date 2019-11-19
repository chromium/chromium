// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.annotation.TargetApi;
import android.app.DownloadManager;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.provider.MediaStore.MediaColumns;
import android.text.format.DateUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContentUriUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.io.FileInputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.reflect.Field;
import java.lang.reflect.Method;

/**
 * Since the {@link AndroidDownloadManager} can only be accessed from Java, this bridge will
 * transfer all C++ calls over to Java land for making the call to ADM.  This is a one-way bridge,
 * from C++ to Java only.  The Java side of this bridge is not called by other Java code.
 */
@JNINamespace("offline_pages")
public class OfflinePageArchivePublisherBridge {
    private static final String TAG = "Publisher";
    /** Offline pages should not be scanned as for media content. */
    public static final boolean IS_MEDIA_SCANNER_SCANNABLE = false;

    /** We don't want another download notification, since we already made one. */
    public static final boolean SHOW_NOTIFICATION = false;

    /** Mime type to use for Offline Pages. */
    public static final String MIME_TYPE = "multipart/related";

    /** Returns true if DownloadManager is installed on the phone. */
    @CalledByNative
    @VisibleForTesting
    public static boolean isAndroidDownloadManagerInstalled() {
        DownloadManager downloadManager = getDownloadManager();
        return (downloadManager != null);
    }

    /**
     * This is a pass through to the {@link AndroidDownloadManager} function of the same name.
     * @param title The display name for this download.
     * @param description Long description for this download.
     * @param path File system path for this download.
     * @param length Length in bytes of this downloaded item.
     * @param uri The origin of this download.  Used in API 24+ only.
     * @param referer Where this download was refered from.  Used in API 24+ only.
     * @return the download ID of this item as assigned by the download manager.
     */
    @CalledByNative
    @VisibleForTesting
    public static long addCompletedDownload(String title, String description, String path,
            long length, String uri, String referer) {
        try {
            // Call the proper version of the pass through based on the supported API level.
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N) {
                return callAddCompletedDownload(title, description, path, length);
            }

            return callAddCompletedDownload(title, description, path, length, uri, referer);
        } catch (Exception e) {
            // In case of exception, we return a download id of 0.
            Log.d(TAG, "ADM threw while trying to add a download. " + e);
            return 0;
        }
    }

    // Use this pass through before API level 24.
    private static long callAddCompletedDownload(
            String title, String description, String path, long length) {
        DownloadManager downloadManager = getDownloadManager();
        if (downloadManager == null) return 0;

        return downloadManager.addCompletedDownload(title, description, IS_MEDIA_SCANNER_SCANNABLE,
                MIME_TYPE, path, length, SHOW_NOTIFICATION);
    }

    // Use this pass through for API levels 24 and higher.
    @TargetApi(Build.VERSION_CODES.N)
    private static long callAddCompletedDownload(String title, String description, String path,
            long length, String uri, String referer) {
        DownloadManager downloadManager = getDownloadManager();
        if (downloadManager == null) return 0;

        return downloadManager.addCompletedDownload(title, description, IS_MEDIA_SCANNER_SCANNABLE,
                MIME_TYPE, path, length, SHOW_NOTIFICATION, Uri.parse(uri), Uri.parse(referer));
    }

    /**
     * This is a pass through to the {@link AndroidDownloadManager} function of the same name.
     * @param ids An array of download IDs to be removed from the download manager.
     * @return the number of IDs that were removed.
     */
    @CalledByNative
    @VisibleForTesting
    public static int remove(long[] ids) {
        DownloadManager downloadManager = getDownloadManager();
        try {
            if (downloadManager == null) return 0;

            return downloadManager.remove(ids);
        } catch (Exception e) {
            Log.d(TAG, "ADM threw while trying to remove a download. " + e);
            return 0;
        }
    }

    private static DownloadManager getDownloadManager() {
        return (DownloadManager) ContextUtils.getApplicationContext().getSystemService(
                Context.DOWNLOAD_SERVICE);
    }

    /**
     * Adds an archive to the downloads collection on Android Q+. Preferred alternative to
     * addCompletedDownload for Android Q and later.
     *
     * TODO(iwells): Remove reflection once API level 29 is supported.
     *
     * @param page Offline page to be published.
     * @return Content URI referring to the published page.
     */
    @CalledByNative
    @VisibleForTesting
    public static String publishArchiveToDownloadsCollection(OfflinePageItem page) {
        assert org.chromium.base.BuildInfo.isAtLeastQ();

        final String isPending = "is_pending"; // MediaStore.IS_PENDING

        // Collect all fields for creating intermediate URI.
        final long now = System.currentTimeMillis() / 1000;
        ContentValues pendingValues = new ContentValues();
        pendingValues.put(MediaColumns.DATE_ADDED, now);
        pendingValues.put(MediaColumns.DATE_MODIFIED, now);
        pendingValues.put(isPending, 1);
        pendingValues.put("download_uri", page.getUrl()); // MediaStore.DownloadColumns.DOWNLOAD_URI

        Uri externalDownloadUri;
        try {
            // Class android.provider.MediaStore.Downloads added in API level 29.
            Class<?> downloadsClazz = Class.forName("android.provider.MediaStore$Downloads");
            Field externalUriField = downloadsClazz.getDeclaredField("EXTERNAL_CONTENT_URI");
            externalDownloadUri = (Uri) externalUriField.get(null);
            Field primaryDirectoryField = MediaColumns.class.getDeclaredField("PRIMARY_DIRECTORY");
            pendingValues.put(
                    (String) primaryDirectoryField.get(null), Environment.DIRECTORY_DOWNLOADS);
        } catch (Exception e) {
            Log.d(TAG, "Unable to set pending download fields.", e);
            return "";
        }

        // Pending URI will expire after 3 days.
        long newExpirationTime = (System.currentTimeMillis() + 3 * DateUtils.DAY_IN_MILLIS) / 1000;
        pendingValues.put("date_expires", newExpirationTime);

        // Create intermediate URI.
        ContentResolver contentResolver = ContextUtils.getApplicationContext().getContentResolver();
        Uri intermediateUri = contentResolver.insert(externalDownloadUri, pendingValues);
        if (intermediateUri == null || !ContentUriUtils.isContentUri(intermediateUri.toString())) {
            Log.d(TAG, "Failed to create intermediate URI.");
            return "";
        }

        // Copy archive to intermediate URI.
        try {
            // Class android.os.FileUtils added in API level 29.
            Class<?> fileUtilsClazz = Class.forName("android.os.FileUtils");
            Method copyMethod =
                    fileUtilsClazz.getMethod("copy", InputStream.class, OutputStream.class);

            OutputStream out = contentResolver.openOutputStream(intermediateUri);
            InputStream in = new FileInputStream(page.getFilePath());
            copyMethod.invoke(null, in, out);
            in.close();
            out.close();
        } catch (Exception e) {
            Log.d(TAG, "Unable to copy archive to pending URI.", e);
            return "";
        }

        // Set display name, MIME type, and pending -> false.
        final ContentValues publishValues = new ContentValues();
        publishValues.put(isPending, 0);
        publishValues.putNull("date_expires");
        publishValues.put(MediaColumns.DISPLAY_NAME, page.getTitle());
        publishValues.put(MediaColumns.MIME_TYPE, "multipart/related");
        if (contentResolver.update(intermediateUri, publishValues, null, null) != 1) {
            Log.d(TAG, "Failed to finish publishing archive.");
        }

        // Android Q's MediaStore.Downloads has an issue that the custom mime type which is not
        // supported by MimeTypeMap is overridden to "application/octet-stream" when publishing.
        // To deal with this issue we set the mime type again after publishing.
        // See crbug.com/1010829 for more details.
        final ContentValues mimeTypeValues = new ContentValues();
        mimeTypeValues.put(MediaColumns.MIME_TYPE, "multipart/related");
        if (contentResolver.update(intermediateUri, mimeTypeValues, null, null) != 1) {
            Log.d(TAG, "Failed to update mime type.");
        }
        return intermediateUri.toString();
    }
}
