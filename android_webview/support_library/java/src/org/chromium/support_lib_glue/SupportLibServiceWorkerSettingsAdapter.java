// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import static org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.recordApiCall;

import org.chromium.android_webview.AwServiceWorkerSettings;
import org.chromium.support_lib_boundary.ServiceWorkerWebSettingsBoundaryInterface;
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
}
