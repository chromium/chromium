// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import static org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.recordApiCall;

import org.chromium.android_webview.AwSupportLibIsomorphic;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.TraceEvent;
import org.chromium.support_lib_boundary.WebViewBackForwardCacheSettingsBoundaryInterface;
import org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.ApiCall;

/**
 * Adapter between WebViewBackForwardCacheSettingsBoundaryInterface and AwBackForwardCacheSettings.
 */
@Lifetime.Temporary
class SupportLibWebViewBackForwardCacheSettingsAdapter extends IsomorphicAdapter
        implements WebViewBackForwardCacheSettingsBoundaryInterface {

    private final int mMaxPagesInCache;
    private final int mTimeoutInSeconds;

    SupportLibWebViewBackForwardCacheSettingsAdapter(int maxPagesInCache, int timeoutInSeconds) {
        mMaxPagesInCache = maxPagesInCache;
        mTimeoutInSeconds = timeoutInSeconds;
    }

    @Override
    AwSupportLibIsomorphic getPeeredObject() {
        return null;
    }

    @Override
    public int getTimeoutInSeconds() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.AndroidX.BACK_FORWARD_CACHE_SETTINGS_GET_TIMEOUT_IN_SECONDS")) {
            recordApiCall(ApiCall.BACK_FORWARD_CACHE_SETTINGS_GET_TIMEOUT_IN_SECONDS);
            return mTimeoutInSeconds;
        }
    }

    @Override
    public int getMaxPagesInCache() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.AndroidX.BACK_FORWARD_CACHE_SETTINGS_GET_MAX_PAGES_IN_CACHE")) {
            recordApiCall(ApiCall.BACK_FORWARD_CACHE_SETTINGS_GET_MAX_PAGES_IN_CACHE);
            return mMaxPagesInCache;
        }
    }
}
