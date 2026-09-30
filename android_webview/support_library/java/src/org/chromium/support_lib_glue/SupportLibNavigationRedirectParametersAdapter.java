// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import static org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.recordApiCall;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.TraceEvent;
import org.chromium.build.annotations.NullMarked;
import org.chromium.support_lib_boundary.NavigationRedirectParametersBoundaryInterface;
import org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.ApiCall;

import java.util.Map;

/**
 * Adapter between NavigationRedirectParametersBoundaryInterface and the details of a redirect.
 *
 * <p>A new instance is created for each redirect, the object is only valid for the duration of the
 * callback it is passed to.
 */
@Lifetime.Temporary
@NullMarked
class SupportLibNavigationRedirectParametersAdapter
        implements NavigationRedirectParametersBoundaryInterface {
    private final Map<String, String> mResponseHeaders;
    private final int mStatusCode;

    SupportLibNavigationRedirectParametersAdapter(
            Map<String, String> responseHeaders, int statusCode) {
        mResponseHeaders = responseHeaders;
        mStatusCode = statusCode;
    }

    @Override
    public Map<String, String> getResponseHeaders() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.AndroidX.REDIRECT_PARAMETERS_GET_RESPONSE_HEADERS")) {
            recordApiCall(ApiCall.REDIRECT_PARAMETERS_GET_RESPONSE_HEADERS);
            return mResponseHeaders;
        }
    }

    @Override
    public int getStatusCode() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.REDIRECT_PARAMETERS_GET_STATUS_CODE")) {
            recordApiCall(ApiCall.REDIRECT_PARAMETERS_GET_STATUS_CODE);
            return mStatusCode;
        }
    }
}
