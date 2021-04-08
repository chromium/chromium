// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_annotations;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.Callback;
import org.chromium.base.LocaleUtils;
import org.chromium.base.Log;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointFetcher;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointResponse;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/**
 * Contains the business logic to call the Page Annotations backend over HTTPS.
 */
public class PageAnnotationsServiceProxy {
    private static final String TAG = "PASP";
    private static final String GET_ANNOTATIONS_QUERY_PARAMS_TEMPLATE = "?url=%s";
    private static final long TIMEOUT_MS = 1000L;
    private static final String HTTPS_METHOD = "GET";
    private static final String CONTENT_TYPE = "application/json; charset=UTF-8";
    private static final String EMPTY_POST_DATA = "";
    private static final String ANNOTATIONS_KEY = "annotations";
    private static final String ACCEPT_LANGUAGE_KEY = "Accept-Language";
    private final Profile mProfile;

    /**
     * Creates a new proxy instance.
     *
     * @param profile Profile to use for auth.
     */
    PageAnnotationsServiceProxy(Profile profile) {
        mProfile = profile;
    }

    /**
     * Makes an HTTPS call to the backend and returns the service response through
     * the provided callback.
     *
     * @param url      The URL to annotate.
     * @param callback {@link Callback} to invoke once the request is complete.
     */
    public void fetchAnnotations(
            GURL url, Callback<SinglePageAnnotationsServiceResponse> callback) {
        if (url == null || url.isEmpty()) {
            callback.onResult(null);
            return;
        }

        EndpointFetcher.fetchUsingChromeAPIKey(
                (endpointResponse)
                        -> { fetchCallback(endpointResponse, callback); },
                mProfile,
                String.format(PageAnnotationsServiceConfig.PAGE_ANNOTATIONS_BASE_URL.getValue()
                                + GET_ANNOTATIONS_QUERY_PARAMS_TEMPLATE,
                        UrlUtilities.escapeQueryParamValue(url.getSpec(), false)),
                HTTPS_METHOD, CONTENT_TYPE, EMPTY_POST_DATA, TIMEOUT_MS,
                new String[] {ACCEPT_LANGUAGE_KEY, LocaleUtils.getDefaultLocaleListString()});
    }

    private void fetchCallback(EndpointResponse response,
            Callback<SinglePageAnnotationsServiceResponse> resultCallback) {
        List<PageAnnotation> annotations = new ArrayList<>();
        String responseString = response.getResponseString();
        try {
            JSONObject responseJson = new JSONObject(responseString);
            JSONArray annotationsJson = responseJson.getJSONArray(ANNOTATIONS_KEY);

            for (int i = 0; i < annotationsJson.length(); i++) {
                PageAnnotation annotation = PageAnnotationUtils.createPageAnnotationFromJson(
                        annotationsJson.getJSONObject(i));
                if (annotation != null) {
                    annotations.add(annotation);
                }
            }
        } catch (JSONException e) {
            Log.e(TAG,
                    "Failed to parse SingleUrlPageAnnotations response. Details: " + e.toString());
        }

        resultCallback.onResult(new SinglePageAnnotationsServiceResponse(annotations));
    }
}