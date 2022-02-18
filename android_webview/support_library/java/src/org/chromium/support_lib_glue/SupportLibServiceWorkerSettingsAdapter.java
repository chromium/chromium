// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import static org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.recordApiCall;

import org.chromium.android_webview.AwServiceWorkerSettings;
import org.chromium.android_webview.AwSettings;
import org.chromium.support_lib_boundary.ServiceWorkerWebSettingsBoundaryInterface;
import org.chromium.support_lib_boundary.WebSettingsBoundaryInterface.RequestedWithHeaderMode;
import org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.ApiCall;

/**
 * Adapter between AwServiceWorkerSettings and ServiceWorkerWebSettingsBoundaryInterface.
 */
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
        recordApiCall(ApiCall.SERVICE_WORKER_SETTINGS_SET_CACHE_MODE);
        mAwServiceWorkerSettings.setCacheMode(mode);
    }

    @Override
    public int getCacheMode() {
        recordApiCall(ApiCall.SERVICE_WORKER_SETTINGS_GET_CACHE_MODE);
        return mAwServiceWorkerSettings.getCacheMode();
    }

    @Override
    public void setAllowContentAccess(boolean allow) {
        recordApiCall(ApiCall.SERVICE_WORKER_SETTINGS_SET_ALLOW_CONTENT_ACCESS);
        mAwServiceWorkerSettings.setAllowContentAccess(allow);
    }

    @Override
    public boolean getAllowContentAccess() {
        recordApiCall(ApiCall.SERVICE_WORKER_SETTINGS_GET_ALLOW_CONTENT_ACCESS);
        return mAwServiceWorkerSettings.getAllowContentAccess();
    }

    @Override
    public void setAllowFileAccess(boolean allow) {
        recordApiCall(ApiCall.SERVICE_WORKER_SETTINGS_SET_ALLOW_FILE_ACCESS);
        mAwServiceWorkerSettings.setAllowFileAccess(allow);
    }

    @Override
    public boolean getAllowFileAccess() {
        recordApiCall(ApiCall.SERVICE_WORKER_SETTINGS_GET_ALLOW_FILE_ACCESS);
        return mAwServiceWorkerSettings.getAllowFileAccess();
    }

    @Override
    public void setBlockNetworkLoads(boolean flag) {
        recordApiCall(ApiCall.SERVICE_WORKER_SETTINGS_SET_BLOCK_NETWORK_LOADS);
        mAwServiceWorkerSettings.setBlockNetworkLoads(flag);
    }

    @Override
    public boolean getBlockNetworkLoads() {
        recordApiCall(ApiCall.SERVICE_WORKER_SETTINGS_GET_BLOCK_NETWORK_LOADS);
        return mAwServiceWorkerSettings.getBlockNetworkLoads();
    }

    @Override
    public void setRequestedWithHeaderMode(int mode) {
        recordApiCall(ApiCall.SERVICE_WORKER_SETTINGS_SET_REQUESTED_WITH_HEADER_MODE);
        switch (mode) {
            case RequestedWithHeaderMode.NO_HEADER:
                mAwServiceWorkerSettings.setRequestedWithHeaderMode(
                        AwSettings.REQUESTED_WITH_NO_HEADER);
                break;
            case RequestedWithHeaderMode.APP_PACKAGE_NAME:
                mAwServiceWorkerSettings.setRequestedWithHeaderMode(
                        AwSettings.REQUESTED_WITH_APP_PACKAGE_NAME);
                break;
        }
    }

    @Override
    public int getRequestedWithHeaderMode() {
        recordApiCall(ApiCall.SERVICE_WORKER_SETTINGS_GET_REQUESTED_WITH_HEADER_MODE);
        // The AwSettings.REQUESTED_WITH_CONSTANT_WEBVIEW setting is intended to be internal
        // and for testing only, so it will not be mapped in the public API.
        switch (mAwServiceWorkerSettings.getRequestedWithHeaderMode()) {
            case AwSettings.REQUESTED_WITH_NO_HEADER:
                return RequestedWithHeaderMode.NO_HEADER;
            case AwSettings.REQUESTED_WITH_APP_PACKAGE_NAME:
                return RequestedWithHeaderMode.APP_PACKAGE_NAME;
        }
        return RequestedWithHeaderMode.APP_PACKAGE_NAME;
    }
}
