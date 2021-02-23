// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.nonembedded;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.url.GURL;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;

/**
 * This class  is responsible for downloading a file, and is to be used by WebView nonembedded
 * processes where Chromium's network service can't be used to do network operations.
 */
@JNINamespace("android_webview")
public class DownloadFileTask {
    private static final String TAG = "AWDownloadFileTask";
    private static final int READ_TIMEOUT_MS = 30000;
    private static final int BUFFER_SIZE_BYTES = 1024;

    @CalledByNative
    private static void download(
            long nativeDownloadFileTask, long mainTaskRunner, GURL url, String filePath) {
        downloadToFile(
                /* connection= */ null, nativeDownloadFileTask, mainTaskRunner, url, filePath);
    }

    /**
     * Downloads from a given url to a file.
     */
    @VisibleForTesting
    static void downloadToFile(HttpURLConnection connection, long nativeDownloadFileTask,
            long mainTaskRunner, GURL gurl, String filePath) {
        long bytesDownloaded = 0;
        try {
            if (connection == null) {
                URL url = new URL(gurl.getSpec());
                connection = (HttpURLConnection) url.openConnection();
            }

            File outputFile = new File(filePath);
            if (!outputFile.exists()) {
                throw new FileNotFoundException("File to download contents to does not exist.");
            }

            long contentLength = 0;
            String contentLengthString = connection.getHeaderField("Content-Length");
            if (contentLengthString != null && !contentLengthString.isEmpty()) {
                contentLength = Long.parseLong(contentLengthString);
            }

            DownloadFileTaskJni.get().callResponseStartedCallback(nativeDownloadFileTask,
                    mainTaskRunner,
                    /* responseCode= */ connection.getResponseCode(), contentLength);

            connection.setConnectTimeout(READ_TIMEOUT_MS);

            InputStream inputStream = connection.getInputStream();
            OutputStream outputStream = new FileOutputStream(outputFile, /* append= */ true);
            byte[] buffer = new byte[BUFFER_SIZE_BYTES];

            int bytesCount;
            while ((bytesCount = inputStream.read(buffer)) > 0) {
                outputStream.write(buffer, 0, bytesCount);
                bytesDownloaded += bytesCount;
                DownloadFileTaskJni.get().callProgressCallback(
                        nativeDownloadFileTask, mainTaskRunner, bytesDownloaded);
            }

            DownloadFileTaskJni.get().callDownloadToFileCompleteCallback(
                    nativeDownloadFileTask, mainTaskRunner, /* networkError= */ 0, bytesDownloaded);
        } catch (IOException exception) {
            Log.w(TAG, "IOException during downloadToFile.", exception);

            // Notify task completion with a generic error.
            DownloadFileTaskJni.get().callDownloadToFileCompleteCallback(nativeDownloadFileTask,
                    mainTaskRunner, /* networkError= */ -2, bytesDownloaded);
        } finally {
            if (connection != null) {
                connection.disconnect();
            }
        }
    }

    private DownloadFileTask() {
        // Private constructor to prevent the object from being created.
    }

    @NativeMethods
    interface Natives {
        void callProgressCallback(long weakPtr, long taskRunner, long current);
        void callResponseStartedCallback(
                long weakPtr, long taskRunner, int responseCode, long contentLength);
        void callDownloadToFileCompleteCallback(
                long weakPtr, long taskRunner, int networkError, long contentSize);
    } // interface Natives
}
