// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import org.chromium.android_webview.AwServiceWorkerSettings;

/**
 * Type adaptation layer between {@link android.webkit.ServiceWorkerWebSettings}
 * and {@link org.chromium.android_webview.AwServiceWorkerSettings}.
 */
@SuppressWarnings("NoSynchronizedMethodCheck")
public class ServiceWorkerSettingsAdapter extends android.webkit.ServiceWorkerWebSettings {
    private AwServiceWorkerSettings mAwServiceWorkerSettings;

    public ServiceWorkerSettingsAdapter(AwServiceWorkerSettings awSettings) {
        mAwServiceWorkerSettings = awSettings;
    }

    AwServiceWorkerSettings getAwSettings() {
        return mAwServiceWorkerSettings;
    }

    @Override
    public void setAllowFileAccess(boolean allow) {
        mAwServiceWorkerSettings.setAllowFileAccess(allow);
    }

    @Override
    public boolean getAllowFileAccess() {
        return mAwServiceWorkerSettings.getAllowFileAccess();
    }

    @Override
    public void setAllowContentAccess(boolean allow) {
        mAwServiceWorkerSettings.setAllowContentAccess(allow);
    }

    @Override
    public boolean getAllowContentAccess() {
        return mAwServiceWorkerSettings.getAllowContentAccess();
    }

    @Override
    public synchronized void setBlockNetworkLoads(boolean flag) {
        mAwServiceWorkerSettings.setBlockNetworkLoads(flag);
    }

    @Override
    public synchronized boolean getBlockNetworkLoads() {
        return mAwServiceWorkerSettings.getBlockNetworkLoads();
    }

    @Override
    public void setCacheMode(int mode) {
        mAwServiceWorkerSettings.setCacheMode(mode);
    }

    @Override
    public int getCacheMode() {
        return mAwServiceWorkerSettings.getCacheMode();
    }
}
