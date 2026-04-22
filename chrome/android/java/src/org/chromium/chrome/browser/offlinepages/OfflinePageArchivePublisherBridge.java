// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.app.DownloadManager;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.net.Uri;
import android.os.Environment;
import android.os.FileUtils;
import android.provider.MediaStore;
import android.provider.MediaStore.MediaColumns;
import android.text.format.DateUtils;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.base.ContentUriUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;

import java.io.FileInputStream;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * Since the {@link AndroidDownloadManager} can only be accessed from Java, this bridge will
 * transfer all C++ calls over to Java land for making the call to ADM. This is a one-way bridge,
 * from C++ to Java only. The Java side of this bridge is not called by other Java code.
 */
@JNINamespace("offline_pages")
@NullMarked
public class OfflinePageArchivePublisherBridge {
    private static final String TAG = "OPArchivePublisher";

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
     *
     * @param title The display name for this download.
     * @param description Long description for this download.
     * @param path File system path for this download.
     * @param length Length in bytes of this downloaded item.
     * @param uri The origin of this download. Used in API 24+ only.
     * @param referer Where this download was refered from. Used in API 24+ only.
     * @return the download ID of this item as assigned by the download manager.
     */
    @CalledByNative
    @VisibleForTesting
    public static long addCompletedDownload(
            @JniType("std::string") String title,
            @JniType("std::string") String description,
            @JniType("std::string") String path,
            long length,
            @JniType("std::string") String uri,
            @JniType("std::string") String referer) {
        try {
            return callAddCompletedDownload(title, description, path, length, uri, referer);
        } catch (Exception e) {
            // In case of exception, we return a download id of 0.
            Log.i(TAG, "ADM threw while trying to add a download. " + e);
            return 0;
        }
    }

    private static long callAddCompletedDownload(
            String title,
            String description,
            String path,
            long length,
            String uri,
            String referer) {
        DownloadManager downloadManager = getDownloadManager();
        if (downloadManager == null) return 0;

        return downloadManager.addCompletedDownload(
                title,
                description,
                IS_MEDIA_SCANNER_SCANNABLE,
                MIME_TYPE,
                path,
                length,
                SHOW_NOTIFICATION,
                Uri.parse(uri),
                Uri.parse(referer));
    }

    /**
     * This is a pass through to the {@link AndroidDownloadManager} function of the same name.
     *
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
            Log.i(TAG, "ADM threw while trying to remove a download. " + e);
            return 0;
        }
    }

    private static DownloadManager getDownloadManager() {
        return (DownloadManager)
                ContextUtils.getApplicationContext().getSystemService(Context.DOWNLOAD_SERVICE);
    }

    /**
     * Adds an archive to the downloads collection on Android Q+. Preferred alternative to
     * addCompletedDownload for Android Q and later.
     *
     * @param page Offline page to be published.
     * @return Content URI referring to the published page.
     */
    @CalledByNative
    @VisibleForTesting
    public static @JniType("std::string") String publishArchiveToDownloadsCollection(
            OfflinePageItem page) {
        // Collect all fields for creating intermediate URI.
        final long now = System.currentTimeMillis() / 1000;
        ContentValues pendingValues = new ContentValues();
        pendingValues.put(MediaColumns.DATE_ADDED, now);
        pendingValues.put(MediaColumns.DATE_MODIFIED, now);
        pendingValues.put(MediaColumns.IS_PENDING, 1);
        pendingValues.put(MediaStore.DownloadColumns.DOWNLOAD_URI, page.getUrl());
        pendingValues.put(MediaColumns.RELATIVE_PATH, Environment.DIRECTORY_DOWNLOADS);

        Uri externalDownloadUri = MediaStore.Downloads.EXTERNAL_CONTENT_URI;

        // Pending URI will expire after 3 days.
        long newExpirationTime = (System.currentTimeMillis() + 3 * DateUtils.DAY_IN_MILLIS) / 1000;
        pendingValues.put(MediaColumns.DATE_EXPIRES, newExpirationTime);

        // Create intermediate URI.
        ContentResolver contentResolver = ContextUtils.getApplicationContext().getContentResolver();
        Uri intermediateUri = contentResolver.insert(externalDownloadUri, pendingValues);
        if (intermediateUri == null || !ContentUriUtils.isContentUri(intermediateUri.toString())) {
            Log.i(TAG, "Failed to create intermediate URI.");
            return "";
        }

        // Copy archive to intermediate URI.
        try (InputStream in = new FileInputStream(page.getFilePath());
                OutputStream out = contentResolver.openOutputStream(intermediateUri)) {
            assert out != null;
            FileUtils.copy(in, out);
        } catch (Exception e) {
            Log.i(
                    TAG,
                    "Unable to copy archive to pending URI (externalDownloadUri: "
                            + externalDownloadUri
                            + ", intermediateUri: "
                            + intermediateUri
                            + ", page.getFilePath(): "
                            + page.getFilePath()
                            + ")",
                    e);
            return "";
        }

        // Set display name, MIME type, and pending -> false.
        final ContentValues publishValues = new ContentValues();
        publishValues.put(MediaColumns.IS_PENDING, 0);
        publishValues.putNull(MediaColumns.DATE_EXPIRES);
        publishValues.put(MediaColumns.DISPLAY_NAME, page.getTitle());
        publishValues.put(MediaColumns.MIME_TYPE, "multipart/related");
        if (!updateContentResolver(
                contentResolver,
                intermediateUri,
                publishValues,
                "Failed to finish publishing archive.")) {
            return "";
        }

        // Android Q's MediaStore.Downloads has an issue that the custom mime type which is not
        // supported by MimeTypeMap is overridden to "application/octet-stream" when publishing.
        // To deal with this issue we set the mime type again after publishing.
        // See crbug.com/40101648 for more details.
        final ContentValues mimeTypeValues = new ContentValues();
        mimeTypeValues.put(MediaColumns.MIME_TYPE, "multipart/related");
        if (!updateContentResolver(
                contentResolver, intermediateUri, mimeTypeValues, "Failed to update mime type.")) {
            return "";
        }
        return intermediateUri.toString();
    }

    private static boolean updateContentResolver(
            ContentResolver contentResolver,
            Uri uri,
            ContentValues contentValues,
            String errorMessage) {
        /* Even though the documentation for ContentResolver.update doesn't mention it, an
         * IllegalStateException (and other RuntimeException's) may be thrown in some situations.
         * This is the case, for instance, when there is a long enough sequence of similarly named
         * files and Android code refuses to generate a new unique filename. See
         * https://crbug.com/40651389 for more details.
         */
        try {
            if (contentResolver.update(uri, contentValues, null, null) == 1) return true;
            Log.i(TAG, errorMessage);
        } catch (RuntimeException e) {
            Log.e(TAG, errorMessage, e);
        }
        return false;
    }
}
