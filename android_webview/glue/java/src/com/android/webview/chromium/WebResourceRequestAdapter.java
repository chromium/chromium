// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.net.Uri;
import android.webkit.WebResourceRequest;

import org.chromium.android_webview.AwContentsClient.AwWebResourceRequest;

import java.util.Map;

/** Adapter between WebResourceRequest and AwWebResourceRequest. */
public class WebResourceRequestAdapter implements WebResourceRequest {
    private final AwWebResourceRequest mRequest;

    public WebResourceRequestAdapter(AwWebResourceRequest request) {
        mRequest = request;
    }

    /* package */ AwWebResourceRequest getAwResourceRequest() {
        return mRequest;
    }

    @Override
    public Uri getUrl() {
        return Uri.parse(mRequest.url);
    }

    @Override
    public boolean isForMainFrame() {
        return mRequest.isOutermostMainFrame;
    }

    @Override
    public boolean hasGesture() {
        return mRequest.hasUserGesture;
    }

    @Override
    public String getMethod() {
        return mRequest.method;
    }

    @Override
    public Map<String, String> getRequestHeaders() {
        return mRequest.requestHeaders;
    }

    @Override
    public boolean isRedirect() {
        return mRequest.isRedirect;
    }
}
