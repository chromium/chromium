// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.browser.profiles.Profile;

import java.io.IOException;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.ProtocolException;
import java.net.SocketTimeoutException;
import java.net.URL;

/**
 * A utility class for checking if the device is currently connected to the Internet.
 */
@JNINamespace("chrome::android")
public final class ConnectivityChecker {
    private static final String TAG = "feedback";

    private static final String DEFAULT_HTTP_NO_CONTENT_URL =
            "http://clients4.google.com/generate_204";
    private static final String DEFAULT_HTTPS_NO_CONTENT_URL =
            "https://clients4.google.com/generate_204";

    private static String sHttpNoContentUrl = DEFAULT_HTTP_NO_CONTENT_URL;
    private static String sHttpsNoContentUrl = DEFAULT_HTTPS_NO_CONTENT_URL;

    /**
     * A callback for whether the device is currently connected to the Internet.
     */
    public interface ConnectivityCheckerCallback {
        /**
         * Called when the result of the connectivity check is ready.
         */
        void onResult(int result);
    }

    @VisibleForTesting
    static void overrideUrlsForTest(String httpUrl, String httpsUrl) {
        ThreadUtils.assertOnUiThread();
        sHttpNoContentUrl = httpUrl;
        sHttpsNoContentUrl = httpsUrl;
    }

    private static void postResult(final ConnectivityCheckerCallback callback, final int result) {
        ThreadUtils.postOnUiThread(new Runnable() {
            @Override
            public void run() {
                callback.onResult(result);
            }
        });
    }

    /**
     * Starts an asynchronous request for checking whether the device is currently connected to the
     * Internet using the Android system network stack. The result passed to the callback denotes
     * whether the attempt to connect to the server was successful.
     *
     * If the profile or URL is invalid, the callback will be called with false.
     * The server reply for the URL must respond with HTTP 204 without any redirects for the
     * connectivity check to be successful.
     *
     * This method takes ownership of the callback object until the callback has happened.
     * This method must be called on the main thread.
     * @param timeoutMs number of milliseconds to wait before giving up waiting for a connection.
     * @param callback the callback which will get the result.
     */
    public static void checkConnectivitySystemNetworkStack(
            boolean useHttps, int timeoutMs, ConnectivityCheckerCallback callback) {
        String url = useHttps ? sHttpsNoContentUrl : sHttpNoContentUrl;
        checkConnectivitySystemNetworkStack(url, timeoutMs, callback);
    }

    static void checkConnectivitySystemNetworkStack(
            String urlStr, final int timeoutMs, final ConnectivityCheckerCallback callback) {
        if (!nativeIsUrlValid(urlStr)) {
            Log.w(TAG, "Predefined URL invalid.");
            postResult(callback, ConnectivityCheckResult.ERROR);
            return;
        }
        final URL url;
        try {
            url = new URL(urlStr);
        } catch (MalformedURLException e) {
            Log.w(TAG, "Failed to parse predefined URL: " + e);
            postResult(callback, ConnectivityCheckResult.ERROR);
            return;
        }
        new AsyncTask<Integer>() {
            @Override
            protected Integer doInBackground() {
                try {
                    HttpURLConnection conn = (HttpURLConnection) url.openConnection();
                    conn.setInstanceFollowRedirects(false);
                    conn.setRequestMethod("GET");
                    conn.setDoInput(false);
                    conn.setDoOutput(false);
                    conn.setConnectTimeout(timeoutMs);
                    conn.setReadTimeout(timeoutMs);

                    conn.connect();
                    int responseCode = conn.getResponseCode();
                    if (responseCode == HttpURLConnection.HTTP_NO_CONTENT) {
                        return ConnectivityCheckResult.CONNECTED;
                    } else {
                        return ConnectivityCheckResult.NOT_CONNECTED;
                    }
                } catch (SocketTimeoutException e) {
                    return ConnectivityCheckResult.TIMEOUT;
                } catch (ProtocolException e) {
                    return ConnectivityCheckResult.ERROR;
                } catch (IOException e) {
                    return ConnectivityCheckResult.NOT_CONNECTED;
                }
            }

            @Override
            protected void onPostExecute(Integer result) {
                callback.onResult(result);
            }
        }
                .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Starts an asynchronous request for checking whether the device is currently connected to the
     * Internet using the Chrome network stack. The result passed to the callback denotes whether
     *the
     * attempt to connect to the server was successful.
     *
     * If the profile or URL is invalid, the callback will be called with false.
     * The server reply for the URL must respond with HTTP 204 without any redirects for the
     * connectivity check to be successful.
     *
     * This method takes ownership of the callback object until the callback has happened.
     * This method must be called on the main thread.
     * @param profile the context to do the check in.
     * @param timeoutMs number of milliseconds to wait before giving up waiting for a connection.
     * @param callback the callback which will get the result.
     */
    public static void checkConnectivityChromeNetworkStack(Profile profile, boolean useHttps,
            int timeoutMs, ConnectivityCheckerCallback callback) {
        String url = useHttps ? sHttpsNoContentUrl : sHttpNoContentUrl;
        checkConnectivityChromeNetworkStack(profile, url, timeoutMs, callback);
    }

    @VisibleForTesting
    static void checkConnectivityChromeNetworkStack(
            Profile profile, String url, long timeoutMs, ConnectivityCheckerCallback callback) {
        ThreadUtils.assertOnUiThread();
        nativeCheckConnectivity(profile, url, timeoutMs, callback);
    }

    @CalledByNative
    private static void executeCallback(Object callback, int result) {
        ((ConnectivityCheckerCallback) callback).onResult(result);
    }

    private ConnectivityChecker() {}

    private static native void nativeCheckConnectivity(
            Profile profile, String url, long timeoutMs, ConnectivityCheckerCallback callback);

    private static native boolean nativeIsUrlValid(String url);
}
