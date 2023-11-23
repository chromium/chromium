// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import static org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.recordApiCall;

import org.chromium.android_webview.AwContentsClient.AwWebResourceRequest;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.TraceEvent;
import org.chromium.support_lib_boundary.WebResourceRequestBoundaryInterface;
import org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.ApiCall;

/** Adapter between AwWebResourceRequest and WebResourceRequestBoundaryInterface. */
@Lifetime.Temporary
public class SupportLibWebResourceRequest implements WebResourceRequestBoundaryInterface {
    private final AwWebResourceRequest mAwRequest;

    SupportLibWebResourceRequest(AwWebResourceRequest request) {
        mAwRequest = request;
    }

    @Override
    public boolean isRedirect() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.WEB_RESOURCE_REQUEST_IS_REDIRECT")) {
            recordApiCall(ApiCall.WEB_RESOURCE_REQUEST_IS_REDIRECT);
            return mAwRequest.isRedirect;
        }
    }
}
