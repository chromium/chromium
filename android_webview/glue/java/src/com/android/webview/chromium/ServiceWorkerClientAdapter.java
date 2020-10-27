// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.annotation.TargetApi;
import android.os.Build;
import android.webkit.ServiceWorkerClient;
import android.webkit.WebResourceResponse;

import org.chromium.android_webview.AwContentsClient.AwWebResourceRequest;
import org.chromium.android_webview.AwServiceWorkerClient;
import org.chromium.components.embedder_support.util.WebResourceResponseInfo;

import java.util.HashMap;
import java.util.Map;

/**
 * An adapter class that forwards the callbacks from {@link AwServiceWorkerClient}
 * to the corresponding {@link ServiceWorkerClient}.
 */
@TargetApi(Build.VERSION_CODES.N)
public class ServiceWorkerClientAdapter extends AwServiceWorkerClient {

    private ServiceWorkerClient mServiceWorkerClient;

    public ServiceWorkerClientAdapter(ServiceWorkerClient client) {
        mServiceWorkerClient = client;
    }

    @Override
    public WebResourceResponseInfo shouldInterceptRequest(AwWebResourceRequest request) {
        WebResourceResponse response =
                mServiceWorkerClient.shouldInterceptRequest(new WebResourceRequestAdapter(request));
        return fromWebResourceResponse(response);
    }

    public static WebResourceResponseInfo fromWebResourceResponse(WebResourceResponse response) {
        if (response == null) return null;

        // WebResourceResponseInfo should support null headers. b/16332774.
        Map<String, String> responseHeaders = response.getResponseHeaders();
        if (responseHeaders == null) responseHeaders = new HashMap<String, String>();

        return new WebResourceResponseInfo(response.getMimeType(), response.getEncoding(),
                response.getData(), response.getStatusCode(), response.getReasonPhrase(),
                responseHeaders);
    }
}
