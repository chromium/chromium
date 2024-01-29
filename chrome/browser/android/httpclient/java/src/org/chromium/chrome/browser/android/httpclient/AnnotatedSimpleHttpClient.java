// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.android.httpclient;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.android.httpclient.SimpleHttpClient.HttpResponse;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.net.NetError;
import org.chromium.net.NetworkTrafficAnnotationTag;
import org.chromium.url.GURL;

import java.util.Map;

/**
 * HttpClient that has a NetworkTrafficAnnotationTag preset for all requests.
 *
 * The main usecase of this class is passing an HttpClient into 3rd party code
 * that does not know about chrome's network stack.
 *
 */
public class AnnotatedSimpleHttpClient implements ChromeHttpClient {
    private final Profile mProfile;
    private final NetworkTrafficAnnotationTag mAnnotation;

    public AnnotatedSimpleHttpClient(Profile profile, NetworkTrafficAnnotationTag annotation) {
        mProfile = profile;
        mAnnotation = annotation;
    }

    @Override
    public void send(
            String url,
            String requestType,
            byte[] body,
            Map<String, String> headers,
            HttpResponseCallback callback) {
        GURL gurl = new GURL(url);
        // Also mask network stack error codes as HTTP status code (better than
        // swallowing it and third_party code does not know about chrome's
        // network stack errors enum).
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    if (mProfile == null || !mProfile.isNativeInitialized()) {
                        return;
                    }

                    SimpleHttpClient.getForProfile(mProfile)
                            .send(
                                    gurl,
                                    requestType,
                                    body,
                                    headers,
                                    mAnnotation,
                                    (HttpResponse response) -> {
                                        callback.accept(
                                                getStatusCode(response),
                                                response.mBody,
                                                response.mHeaders);
                                    });
                });
    }

    private static int getStatusCode(HttpResponse response) {
        // Overload the http status code field to carry network error codes as
        // well. Chrome's network stack error codes are all < 0 so will not
        // conflict with http status codes thus allow us to exfiltrate the cause
        // of the failure without the client library needing to understand our
        // error codes, or handling an extra error code.
        int responseCode =
                response.mNetErrorCode != 0
                                && response.mNetErrorCode != NetError.ERR_HTTP_RESPONSE_CODE_FAILURE
                        ? response.mNetErrorCode
                        : response.mResponseCode;
        return responseCode;
    }
}
