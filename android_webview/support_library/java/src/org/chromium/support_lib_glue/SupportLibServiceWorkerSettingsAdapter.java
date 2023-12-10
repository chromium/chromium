// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import static org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.recordApiCall;

import org.chromium.android_webview.AwServiceWorkerSettings;
import org.chromium.base.TraceEvent;
import org.chromium.support_lib_boundary.ServiceWorkerWebSettingsBoundaryInterface;
import org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.ApiCall;

import java.util.Set;

/** Adapter between AwServiceWorkerSettings and ServiceWorkerWebSettingsBoundaryInterface. */
class SupportLibServiceWorkerSettingsAdapter implements ServiceWorkerWebSettingsBoundaryInterface {
    private AwServiceWorkerSettings mAwServiceWorkerSettings;

    SupportLibServiceWorkerSettingsAdapter(AwServiceWorkerSettings settings) {
        mAwServiceWorkerSettings = settings;
    }

    /* package */ AwServiceWorkerSettings getAwServiceWorkerSettings() {
        return mAwServiceWorkerSettings;
    }

    @Override
    public void setCacheMode(int mode) {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.AndroidX.SERVICE_WORKER_SETTINGS_SET_CACHE_MODE")) {
            recordApiCall(ApiCall.SERVICE_WORKER_SETTINGS_SET_CACHE_MODE);
            mAwServiceWorkerSettings.setCacheMode(mode);
        }
    }

    @Override
    public int getCacheMode() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.AndroidX.SERVICE_WORKER_SETTINGS_GET_CACHE_MODE")) {
            recordApiCall(ApiCall.SERVICE_WORKER_SETTINGS_GET_CACHE_MODE);
            return mAwServiceWorkerSettings.getCacheMode();
        }
    }

    @Override
    public void setAllowContentAccess(boolean allow) {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.AndroidX.SERVICE_WORKER_SETTINGS_SET_ALLOW_CONTENT_ACCESS")) {
            recordApiCall(ApiCall.SERVICE_WORKER_SETTINGS_SET_ALLOW_CONTENT_ACCESS);
            mAwServiceWorkerSettings.setAllowContentAccess(allow);
        }
    }

    @Override
    public boolean getAllowContentAccess() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.AndroidX.SERVICE_WORKER_SETTINGS_GET_ALLOW_CONTENT_ACCESS")) {
            recordApiCall(ApiCall.SERVICE_WORKER_SETTINGS_GET_ALLOW_CONTENT_ACCESS);
            return mAwServiceWorkerSettings.getAllowContentAccess();
        }
    }

    @Override
    public void setAllowFileAccess(boolean allow) {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.AndroidX.SERVICE_WORKER_SETTINGS_SET_ALLOW_FILE_ACCESS")) {
            recordApiCall(ApiCall.SERVICE_WORKER_SETTINGS_SET_ALLOW_FILE_ACCESS);
            mAwServiceWorkerSettings.setAllowFileAccess(allow);
        }
    }

    @Override
    public boolean getAllowFileAccess() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.AndroidX.SERVICE_WORKER_SETTINGS_GET_ALLOW_FILE_ACCESS")) {
            recordApiCall(ApiCall.SERVICE_WORKER_SETTINGS_GET_ALLOW_FILE_ACCESS);
            return mAwServiceWorkerSettings.getAllowFileAccess();
        }
    }

    @Override
    public void setBlockNetworkLoads(boolean flag) {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.AndroidX.SERVICE_WORKER_SETTINGS_SET_BLOCK_NETWORK_LOADS")) {
            recordApiCall(ApiCall.SERVICE_WORKER_SETTINGS_SET_BLOCK_NETWORK_LOADS);
            mAwServiceWorkerSettings.setBlockNetworkLoads(flag);
        }
    }

    @Override
    public boolean getBlockNetworkLoads() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.AndroidX.SERVICE_WORKER_SETTINGS_GET_BLOCK_NETWORK_LOADS")) {
            recordApiCall(ApiCall.SERVICE_WORKER_SETTINGS_GET_BLOCK_NETWORK_LOADS);
            return mAwServiceWorkerSettings.getBlockNetworkLoads();
        }
    }

    @Override
    public void setRequestedWithHeaderOriginAllowList(Set<String> allowedOriginRules) {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.AndroidX.SERVICE_WORKER_SETTINGS_SET_REQUESTED_WITH_HEADER_ORIGIN_ALLOWLIST")) {
            recordApiCall(
                    ApiCall.SERVICE_WORKER_SETTINGS_SET_REQUESTED_WITH_HEADER_ORIGIN_ALLOWLIST);
            mAwServiceWorkerSettings.setRequestedWithHeaderOriginAllowList(allowedOriginRules);
        }
    }

    @Override
    public Set<String> getRequestedWithHeaderOriginAllowList() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.AndroidX.SERVICE_WORKER_SETTINGS_GET_REQUESTED_WITH_HEADER_ORIGIN_ALLOWLIST")) {
            recordApiCall(
                    ApiCall.SERVICE_WORKER_SETTINGS_GET_REQUESTED_WITH_HEADER_ORIGIN_ALLOWLIST);
            return mAwServiceWorkerSettings.getRequestedWithHeaderOriginAllowList();
        }
    }
}
