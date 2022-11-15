// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.android.httpclient;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.JNIUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.net.NetworkTrafficAnnotationTag;
import org.chromium.url.GURL;

import java.util.HashMap;
import java.util.Map;
import java.util.function.Consumer;

/**
 * A very simple http client
 *
 * This client only supports small (<4MB) one way requests/responses. No
 * bidirectional connections or streams support. Only use this if you need a
 * small network request for a java feature with no other native code.
 *
 * If your feature already has a native component, it might be better for you to
 * use chrome's network stack from native within your own native code instead.
 */
@JNINamespace("httpclient")
public class SimpleHttpClient {
    private long mNativeBridge;
    private static SimpleHttpClient sInstance;

    /**
     * Data structure representing HTTP response. Used between native and Java.
     *
     * When there are any error occurs (due to network errors / HTTP errors / invalid requests),
     * the response body will be left empty, and the error code will be populated accordingly.
     */
    public static class HttpResponse {
        /**
         * The response code for the response. If the response fails without receiving headers,
         * this will be 0.
         */
        public final int mResponseCode;

        /** The network error code for the response if failed, otherwise 0. */
        public final int mNetErrorCode;

        /** The response body in bytes presentation. */
        public final byte[] mBody;

        /** The headers for the response. */
        public final Map<String, String> mHeaders;

        HttpResponse(int responseCode, int netErrorCode, byte[] body, Map<String, String> headers) {
            mResponseCode = responseCode;
            mNetErrorCode = netErrorCode;
            mBody = body;
            mHeaders = headers;
        }
    }

    public SimpleHttpClient(Profile profile) {
        mNativeBridge = SimpleHttpClientJni.get().init(profile);
    }

    public static SimpleHttpClient get() {
        if (sInstance == null) {
            sInstance = new SimpleHttpClient(Profile.getLastUsedRegularProfile());
        }
        return sInstance;
    }

    /**
     * Destroy the HTTP client. If there are pending requests sent through this client, the response
     * will be ignored and no callback will be invoked.
     */
    public void destroy() {
        SimpleHttpClientJni.get().destroy(mNativeBridge);
    }

    /**
     * Send a HTTP request with the input information.
     *
     * @param gurl GURL from the HTTP request.
     * @param requestType The request method for the HTTP request (GET / POST / etc.).
     * @param body The request body in byte array for the HTTP request.
     * @param headers The request headers for the HTTP request.
     * @param responseConsumer The {@link Consumer} which will be invoked after the request is send
     *         when some response comes back.
     */
    public void send(GURL gurl, String requestType, byte[] body, Map<String, String> headers,
            NetworkTrafficAnnotationTag annotation, Callback<HttpResponse> responseConsumer) {
        assert mNativeBridge != 0;
        assert gurl.isValid();

        String[] headerKeys = new String[headers.size()];
        String[] headerValues = new String[headerKeys.length];
        JNIUtils.splitMap(headers, headerKeys, headerValues);

        SimpleHttpClientJni.get().sendNetworkRequest(mNativeBridge, gurl, requestType, body,
                headerKeys, headerValues, annotation.getHashCode(), responseConsumer);
    }

    /**
     * Create the HttpResponse object based on set of attributes.  Note that the
     * order of headerValues are designed to be purposefully matching
     * headerKeys, and some headerKey(s) might map to multiple headerValues
     * (which will be joined into one value separated by a new line).
     *
     * @param responseCode Response code for HttpResponse.
     * @param netErrorCode Network error code for HttpResponse.
     * @param body Response body for the HttpResponse.
     * @param headerKeys Keys of the headers for the HttpResponse.
     * @param headerValues Values of the headers for the HttpResponse.
     */
    @VisibleForTesting
    @CalledByNative
    public static HttpResponse createHttpResponse(int responseCode, int netErrorCode, byte[] body,
            String[] headerKeys, String[] headerValues) {
        assert headerKeys.length == headerValues.length;

        Map<String, String> responseHeaders = new HashMap<>();

        for (int i = 0; i < headerKeys.length; i++) {
            if (!responseHeaders.containsKey(headerKeys[i])) {
                responseHeaders.put(headerKeys[i], headerValues[i]);
            } else {
                String headerValue = responseHeaders.get(headerKeys[i]);
                headerValue += "\n" + headerValues[i];
                responseHeaders.put(headerKeys[i], headerValue);
            }
        }
        return new HttpResponse(responseCode, netErrorCode, body, responseHeaders);
    }

    @NativeMethods
    interface Natives {
        long init(Profile profile);
        void destroy(long nativeHttpClientBridge);
        void sendNetworkRequest(long nativeHttpClientBridge, GURL gurl, String requestType,
                byte[] body, String[] headerKeys, String[] headerValues, int annotation,
                Callback<HttpResponse> responseCallback);
    }
}
