// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;

import androidx.annotation.GuardedBy;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.android_webview.safe_browsing.AwSafeBrowsingConfigHelper;
import org.chromium.build.annotations.DoNotInline;
import org.chromium.components.embedder_support.util.WebResourceResponseInfo;

/** Manages clients and settings for Service Workers. */
@Lifetime.Profile
public class AwServiceWorkerController {
    @GuardedBy("mAwServiceWorkerClientLock")
    private AwServiceWorkerClient mServiceWorkerClient;

    @DoNotInline // Native stores this as a weak reference.
    @NonNull
    private final AwContentsIoThreadClient mServiceWorkerIoThreadClient;

    @NonNull private final AwContentsBackgroundThreadClient mServiceWorkerBackgroundThreadClient;
    @NonNull private final AwServiceWorkerSettings mServiceWorkerSettings;
    @NonNull private final AwBrowserContext mBrowserContext;

    // Lock to protect access to the |mServiceWorkerClient|
    private final Object mAwServiceWorkerClientLock = new Object();

    public AwServiceWorkerController(
            @NonNull Context applicationContext, @NonNull AwBrowserContext browserContext) {
        mBrowserContext = browserContext;
        mServiceWorkerSettings = new AwServiceWorkerSettings(applicationContext, mBrowserContext);
        mServiceWorkerBackgroundThreadClient = new ServiceWorkerBackgroundThreadClientImpl();
        mServiceWorkerIoThreadClient = new ServiceWorkerIoThreadClientImpl();
        mBrowserContext.setServiceWorkerIoThreadClient(mServiceWorkerIoThreadClient);
    }

    /** Returns the current settings for Service Worker. */
    public AwServiceWorkerSettings getAwServiceWorkerSettings() {
        return mServiceWorkerSettings;
    }

    /** Set custom client to receive callbacks from Service Workers. Can be null. */
    public void setServiceWorkerClient(@Nullable AwServiceWorkerClient client) {
        synchronized (mAwServiceWorkerClientLock) {
            mServiceWorkerClient = client;
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
        public boolean shouldBlockSpecialFileUrls() {
            return mServiceWorkerSettings.getBlockSpecialFileUrls();
        }

        @Override
        public boolean shouldBlockNetworkLoads() {
            return mServiceWorkerSettings.getBlockNetworkLoads();
        }

        @Override
        public boolean shouldAcceptCookies() {
            return mBrowserContext.getCookieManager().acceptCookie();
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

    private class ServiceWorkerBackgroundThreadClientImpl extends AwContentsBackgroundThreadClient {
        // All methods are called on the background thread.
        @Override
        public WebResourceResponseInfo shouldInterceptRequest(
                AwContentsClient.AwWebResourceRequest request) {
            // TODO: Consider analogy with AwContentsClient, i.e.
            //  - do we need an onloadresource callback?
            //  - do we need to post an error if the response data == null?
            synchronized (mAwServiceWorkerClientLock) {
                return mServiceWorkerClient != null
                        ? mServiceWorkerClient.shouldInterceptRequest(request)
                        : null;
            }
        }
    }
}
