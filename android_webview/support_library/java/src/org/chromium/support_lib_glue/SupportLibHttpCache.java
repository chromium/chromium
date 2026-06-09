// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import org.chromium.android_webview.AwHttpCacheManager;
import org.chromium.base.TraceEvent;
import org.chromium.build.annotations.NullMarked;
import org.chromium.support_lib_boundary.HttpCacheBoundaryInterface;

import java.util.concurrent.Callable;

@NullMarked
class SupportLibHttpCache implements HttpCacheBoundaryInterface {
    private final AwHttpCacheManager mAwHttpCacheManager;

    public SupportLibHttpCache(AwHttpCacheManager awHttpCacheManager) {
        mAwHttpCacheManager = awHttpCacheManager;
    }

    @Override
    public long getDefaultQuotaBytes() {
        SupportLibWebViewChromiumFactory.recordApiCall(
                SupportLibWebViewChromiumFactory.ApiCall.HTTP_CACHE_GET_DEFAULT_QUOTA_BYTES);
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.HTTP_CACHE_GET_DEFAULT_QUOTA_BYTES")) {
            return mAwHttpCacheManager.getDefaultQuotaBytes();
        }
    }

    @Override
    public boolean isUsingDefaultQuota() {
        SupportLibWebViewChromiumFactory.recordApiCall(
                SupportLibWebViewChromiumFactory.ApiCall.HTTP_CACHE_IS_USING_DEFAULT_QUOTA);
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.HTTP_CACHE_IS_USING_DEFAULT_QUOTA")) {
            return mAwHttpCacheManager.isUsingDefaultQuota();
        }
    }

    @Override
    public void useDefaultQuota() {
        SupportLibWebViewChromiumFactory.recordApiCall(
                SupportLibWebViewChromiumFactory.ApiCall.HTTP_CACHE_USE_DEFAULT_QUOTA);
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.HTTP_CACHE_USE_DEFAULT_QUOTA")) {
            mAwHttpCacheManager.useDefaultQuota();
        }
    }

    @Override
    public long getQuotaBytes() {
        SupportLibWebViewChromiumFactory.recordApiCall(
                SupportLibWebViewChromiumFactory.ApiCall.HTTP_CACHE_GET_QUOTA_BYTES);
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.HTTP_CACHE_GET_QUOTA_BYTES")) {
            return mAwHttpCacheManager.getQuotaBytes();
        }
    }

    @Override
    public void setQuotaBytes(long quotaInBytes) {
        SupportLibWebViewChromiumFactory.recordApiCall(
                SupportLibWebViewChromiumFactory.ApiCall.HTTP_CACHE_SET_QUOTA_BYTES);
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.HTTP_CACHE_SET_QUOTA_BYTES")) {
            mAwHttpCacheManager.setQuotaBytes(quotaInBytes);
        }
    }

    @Override
    public Object getOrCreatePeer(Callable<Object> creationCallable) {
        return mAwHttpCacheManager.getOrCreateSupportLibObject(creationCallable);
    }
}
