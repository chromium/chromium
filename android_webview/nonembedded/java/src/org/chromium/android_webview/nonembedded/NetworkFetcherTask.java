// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.nonembedded;

import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Log;
import org.chromium.url.GURL;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;

/**
 * This class  is responsible for doing network operations, and is to be used by WebView nonembedded
 * processes where Chromium's network service can't be used to do network operations.
 */
@JNINamespace("android_webview")
public class NetworkFetcherTask {
    private static final String TAG = "AWNetworkFetcherTask";
    private static final int REQUEST_TIMEOUT = 30000;
    private static final int READ_TIMEOUT_MS = 30000;
    private static final int BUFFER_SIZE_BYTES = 8192;

    private static final String CONTENTTYPE = "Content-Type";

    // The following two headers carry the ECSDA signature of the POST response,
    // if signing has been used. Two headers are used for redundancy purposes.
    // The value of the `X-Cup-Server-Proof` is preferred.
    private static final String ETAG = "ETag";
    private static final String XCUPSERVERPROOF = "X-Cup-Server-Proof";

    // The server uses the optional X-Retry-After header to indicate that the
    // current request should not be attempted again.
    //
    // The value of the header is the number of seconds to wait before trying to
    // do a subsequent update check. Only the values retrieved over HTTPS are
    // trusted.
    private static final String XRETRYAFTER = "X-Retry-After";

    @CalledByNative
    private static void download(
            long nativeDownloadFileTask, long mainTaskRunner, GURL url, String filePath) {
        downloadToFile(
                /* connection= */ null, nativeDownloadFileTask, mainTaskRunner, url, filePath);
    }

    /** Downloads from a given url to a file. */
    @VisibleForTesting
    public static void downloadToFile(
            HttpURLConnection connection,
            long nativeDownloadFileTask,
            long mainTaskRunner,
            GURL gurl,
            String filePath) {
        long bytesDownloaded = 0;
        try {
            if (connection == null) {
                URL url = new URL(gurl.getSpec());
                connection = (HttpURLConnection) url.openConnection();
            }

            File outputFile = new File(filePath);
            long contentLength = 0;
            String contentLengthString = connection.getHeaderField("Content-Length");
            if (contentLengthString != null && !contentLengthString.isEmpty()) {
                contentLength = Long.parseLong(contentLengthString);
            }

            NetworkFetcherTaskJni.get()
                    .callResponseStartedCallback(
                            nativeDownloadFileTask,
                            mainTaskRunner,
                            /* responseCode= */ connection.getResponseCode(),
                            contentLength);

            connection.setConnectTimeout(READ_TIMEOUT_MS);

            try (InputStream inputStream = connection.getInputStream();
                    OutputStream outputStream = new FileOutputStream(outputFile)) {
                byte[] buffer = new byte[BUFFER_SIZE_BYTES];
                int bytesCount;
                while ((bytesCount = inputStream.read(buffer)) > 0) {
                    outputStream.write(buffer, 0, bytesCount);
                    bytesDownloaded += bytesCount;
                    NetworkFetcherTaskJni.get()
                            .callProgressCallback(
                                    nativeDownloadFileTask, mainTaskRunner, bytesDownloaded);
                }
            }

            NetworkFetcherTaskJni.get()
                    .callDownloadToFileCompleteCallback(
                            nativeDownloadFileTask,
                            mainTaskRunner,
                            /* networkError= */ 0,
                            bytesDownloaded);
        } catch (IOException exception) {
            Log.w(TAG, "IOException during downloadToFile.", exception);

            // Notify task completion with a generic error.
            NetworkFetcherTaskJni.get()
                    .callDownloadToFileCompleteCallback(
                            nativeDownloadFileTask,
                            mainTaskRunner,
                            /* networkError= */ -2,
                            bytesDownloaded);
        } finally {
            if (connection != null) {
                connection.disconnect();
            }
        }
    }

    @CalledByNative
    private static void postRequest(
            long nativeNetworkFetcherTask,
            long mainTaskRunner,
            GURL url,
            byte[] postData,
            String contentType,
            String[] headerKeys,
            String[] headerValues) {
        postRequest(
                /* connection= */ null,
                nativeNetworkFetcherTask,
                mainTaskRunner,
                url,
                postData,
                contentType,
                headerKeys,
                headerValues);
    }

    private static String getHeaderFieldOrEmptyStringIfUnset(
            HttpURLConnection connection, String headerName) {
        String headerValueString = connection.getHeaderField(headerName);
        if (headerValueString != null) {
            return headerValueString;
        } else {
            return "";
        }
    }

    private static long getHeaderFieldAsLong(HttpURLConnection connection, String headerName) {
        String headerValueString = connection.getHeaderField(headerName);
        if (!TextUtils.isEmpty(headerValueString)) {
            return Long.parseLong(headerValueString);
        } else {
            return 0L;
        }
    }

    /** Posts a request to a URL. */
    @VisibleForTesting
    public static void postRequest(
            HttpURLConnection connection,
            long nativeNetworkFetcherTask,
            long mainTaskRunner,
            GURL gurl,
            byte[] postData,
            String contentType,
            String[] headerKeys,
            String[] headerValues) {
        String eTag = "";
        String xCupServerProof = "";
        byte[] responseBody = new byte[0];
        long bytesDownloaded = 0;
        long xRetryAfter = 0;

        try {
            if (connection == null) {
                URL url = new URL(gurl.getSpec());
                connection = (HttpURLConnection) url.openConnection();
            }

            connection.setConnectTimeout(REQUEST_TIMEOUT);
            connection.setDoOutput(true);
            connection.setRequestMethod("POST");
            connection.setRequestProperty(CONTENTTYPE, contentType);

            assert (headerKeys.length == headerValues.length);
            for (int i = 0; i < headerKeys.length; i++) {
                connection.setRequestProperty(headerKeys[i], headerValues[i]);
            }

            connection.setFixedLengthStreamingMode(postData.length);
            try (OutputStream outputStream = connection.getOutputStream()) {
                outputStream.write(postData);
            }

            long contentLength = getHeaderFieldAsLong(connection, "Content-Length");

            int responseCode = connection.getResponseCode();
            NetworkFetcherTaskJni.get()
                    .callResponseStartedCallback(
                            nativeNetworkFetcherTask, mainTaskRunner, responseCode, contentLength);

            eTag = getHeaderFieldOrEmptyStringIfUnset(connection, ETAG);
            xCupServerProof = getHeaderFieldOrEmptyStringIfUnset(connection, XCUPSERVERPROOF);
            xRetryAfter = getHeaderFieldAsLong(connection, XRETRYAFTER);

            if (responseCode != HttpURLConnection.HTTP_OK) {
                throw new IOException(
                        "response code is not HTTP_OK. Actual value: " + responseCode);
            }

            connection.setConnectTimeout(READ_TIMEOUT_MS);

            try (InputStream inputStream = connection.getInputStream();
                    ByteArrayOutputStream outBuffer = new ByteArrayOutputStream()) {
                byte[] buffer = new byte[BUFFER_SIZE_BYTES];
                int bytesCount;
                while ((bytesCount = inputStream.read(buffer)) > 0) {
                    bytesDownloaded += bytesCount;
                    outBuffer.write(buffer, /* offset= */ 0, bytesCount);
                    NetworkFetcherTaskJni.get()
                            .callProgressCallback(
                                    nativeNetworkFetcherTask, mainTaskRunner, bytesDownloaded);
                }
                responseBody = outBuffer.toByteArray();
            }

            NetworkFetcherTaskJni.get()
                    .callPostRequestCompleteCallback(
                            nativeNetworkFetcherTask,
                            mainTaskRunner,
                            responseBody,
                            /* networkError= */ 0,
                            eTag,
                            xCupServerProof,
                            xRetryAfter);

        } catch (IOException exception) {
            Log.w(TAG, "IOException during post request.", exception);
            NetworkFetcherTaskJni.get()
                    .callPostRequestCompleteCallback(
                            nativeNetworkFetcherTask,
                            mainTaskRunner,
                            responseBody,
                            /* networkError= */ -2,
                            eTag,
                            xCupServerProof,
                            xRetryAfter);
        } finally {
            if (connection != null) {
                connection.disconnect();
            }
        }
    }

    private NetworkFetcherTask() {
        // Private constructor to prevent the object from being created.
    }

    @NativeMethods
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public interface Natives {
        void callProgressCallback(long weakPtr, long taskRunner, long current);

        void callResponseStartedCallback(
                long weakPtr, long taskRunner, int responseCode, long contentLength);

        void callDownloadToFileCompleteCallback(
                long weakPtr, long taskRunner, int networkError, long contentSize);

        void callPostRequestCompleteCallback(
                long weakPtr,
                long taskRunner,
                byte[] responseBody,
                int networkError,
                String headerETag,
                String headerXCupServerProof,
                long xHeaderRetryAfterSec);
    } // interface Natives
}
