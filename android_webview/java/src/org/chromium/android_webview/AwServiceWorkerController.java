// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;

import org.chromium.android_webview.safe_browsing.AwSafeBrowsingConfigHelper;

/**
 * Manages clients and settings for Service Workers.
 */
public class AwServiceWorkerController {
    private AwServiceWorkerClient mServiceWorkerClient;
    private AwContentsIoThreadClient mServiceWorkerIoThreadClient;
    private AwContentsBackgroundThreadClient mServiceWorkerBackgroundThreadClient;
    private AwServiceWorkerSettings mServiceWorkerSettings;
    private AwBrowserContext mBrowserContext;

    public AwServiceWorkerController(Context applicationContext, AwBrowserContext browserContext) {
        mServiceWorkerSettings = new AwServiceWorkerSettings(applicationContext);
        mBrowserContext = browserContext;
    }

    /**
     * Returns the current settings for Service Worker.
     */
    public AwServiceWorkerSettings getAwServiceWorkerSettings() {
        return mServiceWorkerSettings;
    }

    /**
     * Set custom client to receive callbacks from Service Workers. Can be null.
     */
    public void setServiceWorkerClient(AwServiceWorkerClient client) {
        mServiceWorkerClient = client;
        if (client != null) {
            mServiceWorkerBackgroundThreadClient = new ServiceWorkerBackgroundThreadClientImpl();
            mServiceWorkerIoThreadClient = new ServiceWorkerIoThreadClientImpl();
            AwContentsStatics.setServiceWorkerIoThreadClient(
                    mServiceWorkerIoThreadClient, mBrowserContext);
        } else {
            mServiceWorkerBackgroundThreadClient = null;
            mServiceWorkerIoThreadClient = null;
            AwContentsStatics.setServiceWorkerIoThreadClient(null, mBrowserContext);
        }
    }

    // Helper classes implementations

    private class ServiceWorkerIoThreadClientImpl extends AwContentsIoThreadClient {
        // All methods are called on the IO thread.

        @Override
        public int getCacheMode() {
            return mServiceWorkerSettings.getCacheMode();
        }

        @Override
        public AwContentsBackgroundThreadClient getBackgroundThreadClient() {
            return mServiceWorkerBackgroundThreadClient;
        }

        @Override
        public boolean shouldBlockContentUrls() {
            return !mServiceWorkerSettings.getAllowContentAccess();
        }

        @Override
        public boolean shouldBlockFileUrls() {
            return !mServiceWorkerSettings.getAllowFileAccess();
        }

        @Override
        public boolean shouldBlockNetworkLoads() {
            return mServiceWorkerSettings.getBlockNetworkLoads();
        }

        @Override
        public boolean shouldAcceptThirdPartyCookies() {
            // We currently don't allow third party cookies in service workers,
            // see e.g. AwCookieAccessPolicy::GetShouldAcceptThirdPartyCookies.
            return false;
        }

        @Override
        public boolean getSafeBrowsingEnabled() {
            return AwSafeBrowsingConfigHelper.getSafeBrowsingEnabledByManifest();
        }
    }

    private class ServiceWorkerBackgroundThreadClientImpl
            extends AwContentsBackgroundThreadClient {
        // All methods are called on the background thread.
        @Override
        public AwWebResourceResponse shouldInterceptRequest(
                AwContentsClient.AwWebResourceRequest request) {
            // TODO: Consider analogy with AwContentsClient, i.e.
            //  - do we need an onloadresource callback?
            //  - do we need to post an error if the response data == null?
            return mServiceWorkerClient.shouldInterceptRequest(request);
        }
    }
}
