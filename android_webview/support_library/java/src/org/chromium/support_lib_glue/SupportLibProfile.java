// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import static org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.recordApiCall;

import android.webkit.CookieManager;
import android.webkit.GeolocationPermissions;
import android.webkit.ServiceWorkerController;
import android.webkit.ValueCallback;
import android.webkit.WebStorage;

import androidx.annotation.NonNull;

import com.android.webview.chromium.PrefetchCallback;
import com.android.webview.chromium.Profile;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.support_lib_boundary.PrefetchCallbackBoundaryInterface;
import org.chromium.support_lib_boundary.ProfileBoundaryInterface;
import org.chromium.support_lib_boundary.util.BoundaryInterfaceReflectionUtil;
import org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.ApiCall;

import java.lang.reflect.InvocationHandler;
import java.util.concurrent.Executor;

/** The support-lib glue implementation for Profile, delegates all the calls to {@link Profile}. */
@Lifetime.Profile
public class SupportLibProfile implements ProfileBoundaryInterface {
    private final Profile mProfileImpl;

    public SupportLibProfile(@NonNull Profile profile) {
        mProfileImpl = profile;
    }

    @NonNull
    @Override
    public String getName() {
        recordApiCall(ApiCall.GET_PROFILE_NAME);
        return mProfileImpl.getName();
    }

    @NonNull
    @Override
    public CookieManager getCookieManager() {
        recordApiCall(ApiCall.GET_PROFILE_COOKIE_MANAGER);
        return mProfileImpl.getCookieManager();
    }

    @NonNull
    @Override
    public WebStorage getWebStorage() {
        recordApiCall(ApiCall.GET_PROFILE_WEB_STORAGE);
        return mProfileImpl.getWebStorage();
    }

    @NonNull
    @Override
    public GeolocationPermissions getGeoLocationPermissions() {
        recordApiCall(ApiCall.GET_PROFILE_GEO_LOCATION_PERMISSIONS);
        return mProfileImpl.getGeolocationPermissions();
    }

    @NonNull
    @Override
    public ServiceWorkerController getServiceWorkerController() {
        recordApiCall(ApiCall.GET_PROFILE_SERVICE_WORKER_CONTROLLER);
        return mProfileImpl.getServiceWorkerController();
    }

    @Override
    public void prefetchUrl(
            String url,
            /* PrefetchCallbackBoundaryInterface */ InvocationHandler callbackInvocation,
            Executor callbackExecutor) {
        recordApiCall(ApiCall.PREFETCH_URL);
        PrefetchCallbackBoundaryInterface prefetchCallback =
                BoundaryInterfaceReflectionUtil.castToSuppLibClass(
                        PrefetchCallbackBoundaryInterface.class, callbackInvocation);
        mProfileImpl.prefetchUrl(
                url,
                new PrefetchCallback() {
                    @Override
                    public void onStarted() {
                        prefetchCallback.onStarted();
                    }

                    @Override
                    public void onResponseStarted() {
                        prefetchCallback.onResponseStarted();
                    }

                    @Override
                    public void onResponseHeaderReceived() {
                        prefetchCallback.onResponseHeaderReceived();
                    }

                    @Override
                    public void onCompleted() {
                        prefetchCallback.onCompleted();
                    }

                    @Override
                    public void onFailed(String failureMessage) {
                        prefetchCallback.onFailed(failureMessage);
                    }

                    @Override
                    public void onResponseServed() {
                        prefetchCallback.onResponseServed();
                    }

                    @Override
                    public void onResponseServeFailed(String failureMessage) {
                        prefetchCallback.onResponseServeFailed(failureMessage);
                    }
                },
                callbackExecutor);
    }

    @Override
    public void clearPrefetch(String url, ValueCallback<Void> callback) {
        recordApiCall(ApiCall.CLEAR_PREFETCH_CACHE);
        mProfileImpl.clearPrefetch(url, callback);
    }
}
