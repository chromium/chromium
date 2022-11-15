// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.survey;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.function.Consumer;

/**
 * Class used as HTTP client to send and receive survey-related requests and responses.
 *
 * This class is capable of doing multiple concurrent requests; this class should only be created
 * once for each unique profile.
 *
 * This class should be to be used in UI thread, as it is depending on a UrlLoading which expected
 * to be used on UI thread in native.
 */
@JNINamespace("survey")
public class SurveyHttpClientBridge {
    private long mNativeBridge;

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
        public final Map<String, List<String>> mHeaders;

        HttpResponse(int responseCode, int netErrorCode, byte[] body,
                Map<String, List<String>> headers) {
            mResponseCode = responseCode;
            mNetErrorCode = netErrorCode;
            mBody = body;
            mHeaders = new HashMap<>(headers);
        }
    }

    public SurveyHttpClientBridge(@HttpClientType int clientType, Profile profile) {
        mNativeBridge = SurveyHttpClientBridgeJni.get().init(clientType, profile);
    }

    /**
     * Destroy the HTTP client. If there are pending requests sent through this client, the response
     * will be ignored and no callback will be invoked.
     */
    public void destroy() {
        assert mNativeBridge != 0;
        SurveyHttpClientBridgeJni.get().destroy(mNativeBridge);
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
            Consumer<HttpResponse> responseConsumer) {
        assert mNativeBridge != 0;
        assert gurl.isValid();
        ThreadUtils.assertOnUiThread();

        String[] headerKeys = headers.keySet().toArray(new String[headers.keySet().size()]);
        String[] headerValues = new String[headerKeys.length];

        // While headerKeys are picked up at random orders, headerValue needs to match that order.
        for (int i = 0; i < headerKeys.length; i++) {
            headerValues[i] = headers.get(headerKeys[i]);
        }

        SurveyHttpClientBridgeJni.get().sendNetworkRequest(mNativeBridge, gurl, requestType, body,
                headerKeys, headerValues, responseConsumer::accept);
    }

    /**
     * Create the HttpResponse object based on set of attributes.
     * Note that the order of headerValues are design to be purposefully matching headerKeys, and
     * some headerKey(s) might map to multiple headerValues. These two arrays are seen as key-value
     * pairs and will be parsed into {@link Map<String, List<String>>}.
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

        Map<String, List<String>> responseHeaders = new HashMap<>();

        for (int i = 0; i < headerKeys.length; i++) {
            if (!responseHeaders.containsKey(headerKeys[i])) {
                responseHeaders.put(headerKeys[i], new ArrayList<>());
            }
            responseHeaders.get(headerKeys[i]).add(headerValues[i]);
        }
        return new HttpResponse(responseCode, netErrorCode, body, responseHeaders);
    }

    @NativeMethods
    interface Natives {
        long init(@HttpClientType int clientType, Profile profile);
        void destroy(long nativeSurveyHttpClientBridge);
        void sendNetworkRequest(long nativeSurveyHttpClientBridge, GURL gurl, String requestType,
                byte[] body, String[] headerKeys, String[] headerValues,
                Callback<HttpResponse> resultCallback);
    }
}
