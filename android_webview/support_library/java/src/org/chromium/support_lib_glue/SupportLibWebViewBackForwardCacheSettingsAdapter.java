// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import static org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.recordApiCall;

import org.chromium.android_webview.AwBackForwardCacheSettings;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.TraceEvent;
import org.chromium.support_lib_boundary.WebViewBackForwardCacheSettingsBoundaryInterface;
import org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.ApiCall;

import java.util.concurrent.Callable;

/**
 * Adapter between WebViewBackForwardCacheSettingsBoundaryInterface and AwBackForwardCacheSettings.
 *
 * <p>Once created, instances are kept alive by the peer AwBackForwardCacheSettings.
 */
@Lifetime.Temporary
class SupportLibWebViewBackForwardCacheSettingsAdapter
        implements WebViewBackForwardCacheSettingsBoundaryInterface {
    private final AwBackForwardCacheSettings mAwBackForwardCacheSettings;

    SupportLibWebViewBackForwardCacheSettingsAdapter(AwBackForwardCacheSettings settings) {
        mAwBackForwardCacheSettings = settings;
    }

    SupportLibWebViewBackForwardCacheSettingsAdapter(
            WebViewBackForwardCacheSettingsBoundaryInterface settings) {
        mAwBackForwardCacheSettings =
                new AwBackForwardCacheSettings(
                        settings.getTimeoutInSeconds(), settings.getMaxPagesInCache());
    }

    @Override
    public int getTimeoutInSeconds() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.AndroidX.BACK_FORWARD_CACHE_SETTINGS_GET_TIMEOUT_IN_SECONDS")) {
            recordApiCall(ApiCall.BACK_FORWARD_CACHE_SETTINGS_GET_TIMEOUT_IN_SECONDS);
            return mAwBackForwardCacheSettings.getTimeoutInSeconds();
        }
    }

    @Override
    public int getMaxPagesInCache() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.AndroidX.BACK_FORWARD_CACHE_SETTINGS_GET_MAX_PAGES_IN_CACHE")) {
            recordApiCall(ApiCall.BACK_FORWARD_CACHE_SETTINGS_GET_MAX_PAGES_IN_CACHE);
            return mAwBackForwardCacheSettings.getMaxPagesInCache();
        }
    }

    @Override
    public Object getOrCreatePeer(Callable<Object> creationCallable) {
        return mAwBackForwardCacheSettings.getOrCreateSupportLibObject(creationCallable);
    }
}
