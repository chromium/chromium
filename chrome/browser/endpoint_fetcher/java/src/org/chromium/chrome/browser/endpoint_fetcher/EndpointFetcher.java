// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.endpoint_fetcher;

import androidx.annotation.MainThread;

import org.chromium.base.Callback;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.net.NetworkTrafficAnnotationTag;

/**
 * EndpointFetcher uses native EndpointFetcher to call a HTTPS endpoint and return
 * the response. The call to native EndpointFetcher is achieved over a static call
 * over JNI. The native EndpointFetcher is created during the static call and
 * destroyed in the callback function.
 * EndpointFetcher currently doesn't support incognito mode.
 * If the request times out an empty response will be returned. There will also
 * be an error code indicating timeout once more detailed error messaging is added
 * TODO(crbug.com/993393).
 */
public final class EndpointFetcher {
    private EndpointFetcher() {}

    /**
     * Calls an endpoint using OAuth and returns the response via a callback
     * @param callback callback function response is returned in
     * @param profile profile via which the endpoint is called
     * @param oathConsumerName consumer name for OAuth
     * @param url endpoint URL called
     * @param httpsMethod the HTTPS method used e.g. "GET" or "POST"
     * @param contentType the content type e.g. "application/json"
     * @param scopes scopes used as part of authentication
     * @param postData data for a "POST" request
     * @param timeout time after which the request will terminate in the event a response hasn't
     *         been received
     */
    @MainThread
    public static void fetchUsingOAuth(Callback<EndpointResponse> callback, Profile profile,
            String oathConsumerName, String url, String httpsMethod, String contentType,
            String[] scopes, String postData, long timeout,
            NetworkTrafficAnnotationTag annotation) {
        // EndpointFetcher currently does not support incognito mode
        assert !profile.isOffTheRecord();
        EndpointFetcherJni.get().nativeFetchOAuth(profile, oathConsumerName, url, httpsMethod,
                contentType, scopes, postData, timeout, annotation.getHashCode(), callback);
    }

    /**
     * Calls an endpoint using Chrome API Key returns the response via a callback
     * @param callback to pass the response back in
     * @param profile via which the endpoint is called
     * @param url endpoint URL called
     * @param httpsMethod the HTTPS method used e.g. "GET" or "POST"
     * @param contentType the content type e.g. "application/json"
     * @param postData data for a "POST" request
     * @param timeout time after which the request will terminate in the event a response hasn't
     *         been received
     * @param headers headers for the request. Key/value pairs are stored sequentially in the array.
     */
    @MainThread
    public static void fetchUsingChromeAPIKey(Callback<EndpointResponse> callback, Profile profile,
            String url, String httpsMethod, String contentType, String postData, long timeout,
            String[] headers, NetworkTrafficAnnotationTag annotation) {
        // EndpointFetcher currently does not support incognito mode
        // assert !profile.isOffTheRecord();
        EndpointFetcherJni.get().nativeFetchChromeAPIKey(profile, url, httpsMethod, contentType,
                postData, timeout, headers, annotation.getHashCode(), callback);
    }

    @NativeMethods
    public interface Natives {
        void nativeFetchOAuth(Profile profile, String oathConsumerName, String url,
                String httpsMethod, String contentType, String[] scopes, String postData,
                long timeout, int annotationHashCode, Callback<EndpointResponse> callback);
        void nativeFetchChromeAPIKey(Profile profile, String url, String httpsMethod,
                String contentType, String postData, long timeout, String[] headers,
                int annotationHashCode, Callback<EndpointResponse> callback);
        void nativeFetchWithNoAuth(Profile profile, String url, int annotationHashCode,
                Callback<EndpointResponse> callback);
    }
}
