// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;

import androidx.annotation.GuardedBy;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.safe_browsing.AwSafeBrowsingConfigHelper;
import org.chromium.build.annotations.DoNotInline;
import org.chromium.components.embedder_support.util.WebResourceResponseInfo;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Manages clients and settings for Service Workers.
 */
public class AwServiceWorkerController {
    @GuardedBy("mAwServiceWorkerClientLock")
    private AwServiceWorkerClient mServiceWorkerClient;

    @DoNotInline // Native stores this as a weak reference.
    @NonNull
    private final AwContentsIoThreadClient mServiceWorkerIoThreadClient;
    @NonNull
    private final AwContentsBackgroundThreadClient mServiceWorkerBackgroundThreadClient;
    @NonNull
    private final AwServiceWorkerSettings mServiceWorkerSettings;
    @NonNull
    private final AwBrowserContext mBrowserContext;

    // Lock to protect access to the |mServiceWorkerClient|
    private final Object mAwServiceWorkerClientLock = new Object();

    public AwServiceWorkerController(
            @NonNull Context applicationContext, @NonNull AwBrowserContext browserContext) {
        mBrowserContext = browserContext;
        mServiceWorkerSettings = new AwServiceWorkerSettings(applicationContext, mBrowserContext);
        mServiceWorkerBackgroundThreadClient = new ServiceWorkerBackgroundThreadClientImpl();
        mServiceWorkerIoThreadClient = new ServiceWorkerIoThreadClientImpl();
        AwContentsStatics.setServiceWorkerIoThreadClient(
                mServiceWorkerIoThreadClient, mBrowserContext);
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
        @Override
        public boolean shouldBlockRequest(String url) {
            if (!AwFeatureList.isEnabled(AwFeatures.WEBVIEW_RESTRICT_THIRD_PARTY_CONTENT)) {
                return false;
            }

            CountDownLatch countDownLatch = new CountDownLatch(1);
            AtomicBoolean verified = new AtomicBoolean(false);

            // Verifications are scheduled when WebView is initialized, so when this is called, the
            // verification is likely finished here.
            AwOriginVerificationScheduler scheduler = AwOriginVerificationScheduler.getInstance();
            if (scheduler != null && scheduler.getOriginVerifier().checkForSavedResult(url)) {
                return false;
            }

            AwThreadUtils.postToUiThreadLooper(() -> {
                AwOriginVerificationScheduler.getInstance().verify(url, (result) -> {
                    verified.set(result);
                    countDownLatch.countDown();
                });
            });
            try {
                countDownLatch.await(10, TimeUnit.SECONDS);
            } catch (InterruptedException e) {
                return true;
            }
            return !verified.get();
        }
    }
}
