// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import static org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.recordApiCall;

import org.chromium.android_webview.AwCookieManager;
import org.chromium.base.TraceEvent;
import org.chromium.support_lib_boundary.WebViewCookieManagerBoundaryInterface;

import java.util.List;

/** Adapter between WebViewCookieManagerBoundaryInterface and AwCookieManager. */
class SupportLibWebViewCookieManagerAdapter implements WebViewCookieManagerBoundaryInterface {
    private final AwCookieManager mAwCookieManager;

    public SupportLibWebViewCookieManagerAdapter(AwCookieManager awCookieManager) {
        mAwCookieManager = awCookieManager;
    }

    @Override
    public List<String> getCookieInfo(String url) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.COOKIE_MANAGER_GET_COOKIE_INFO")) {
            recordApiCall(SupportLibWebViewChromiumFactory.ApiCall.COOKIE_MANAGER_GET_COOKIE_INFO);
            return mAwCookieManager.getCookieInfo(url);
        }
    }
}
