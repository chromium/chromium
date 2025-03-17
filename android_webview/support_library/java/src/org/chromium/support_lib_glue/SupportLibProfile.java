// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import static org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.recordApiCall;

import android.os.CancellationSignal;
import android.webkit.CookieManager;
import android.webkit.GeolocationPermissions;
import android.webkit.ServiceWorkerController;
import android.webkit.WebStorage;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.android.webview.chromium.PrefetchOperationCallback;
import com.android.webview.chromium.PrefetchOperationStatusCode;
import com.android.webview.chromium.Profile;
import com.android.webview.chromium.SpeculativeLoadingConfig;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.support_lib_boundary.PrefetchOperationCallbackBoundaryInterface;
import org.chromium.support_lib_boundary.ProfileBoundaryInterface;
import org.chromium.support_lib_boundary.SpeculativeLoadingConfigBoundaryInterface;
import org.chromium.support_lib_boundary.SpeculativeLoadingParametersBoundaryInterface;
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
            CancellationSignal cancellationSignal,
            Executor callbackExecutor,
            /* PrefetchOperationCallback */ InvocationHandler callback) {
        recordApiCall(ApiCall.PREFETCH_URL);
        int prefetchKey =
                mProfileImpl.prefetchUrl(
                        url, null, callbackExecutor, createOperationCallback(callback));
        setCancelListener(cancellationSignal, prefetchKey);
    }

    @Override
    public void prefetchUrl(
            String url,
            @Nullable CancellationSignal cancellationSignal,
            Executor callbackExecutor,
            /* SpeculativeLoadingParameters */ InvocationHandler speculativeLoadingParams,
            /* PrefetchOperationCallback */ InvocationHandler callback) {
        recordApiCall(ApiCall.PREFETCH_URL_WITH_PARAMS);
        SpeculativeLoadingParametersBoundaryInterface speculativeLoadingParameters =
                BoundaryInterfaceReflectionUtil.castToSuppLibClass(
                        SpeculativeLoadingParametersBoundaryInterface.class,
                        speculativeLoadingParams);

        int prefetchKey =
                mProfileImpl.prefetchUrl(
                        url,
                        SupportLibSpeculativeLoadingParametersAdapter
                                .fromSpeculativeLoadingParametersBoundaryInterface(
                                        speculativeLoadingParameters),
                        callbackExecutor,
                        createOperationCallback(callback));

        setCancelListener(cancellationSignal, prefetchKey);
    }

    public void setCancelListener(CancellationSignal cancellationSignal, int prefetchKey) {
        if (cancellationSignal != null) {
            cancellationSignal.setOnCancelListener(
                    () -> {
                        recordApiCall(ApiCall.CANCEL_PREFETCH);
                        mProfileImpl.cancelPrefetch(prefetchKey);
                    });
        }
    }

    @Override
    public void clearPrefetch(
            String url,
            Executor callbackExecutor,
            /* PrefetchOperationCallback */ InvocationHandler callback) {
        recordApiCall(ApiCall.CLEAR_PREFETCH);
        mProfileImpl.clearPrefetch(url, createOperationCallback(callback));
    }

    @Override
    public void setSpeculativeLoadingConfig(
            /* SpeculativeLoadingConfig */ InvocationHandler config) {
        recordApiCall(ApiCall.SET_SPECULATIVE_LOADING_CONFIG);
        SpeculativeLoadingConfigBoundaryInterface speculativeLoadingConfig =
                BoundaryInterfaceReflectionUtil.castToSuppLibClass(
                        SpeculativeLoadingConfigBoundaryInterface.class, config);
        mProfileImpl.setSpeculativeLoadingConfig(
                new SpeculativeLoadingConfig(
                        speculativeLoadingConfig.getMaxPrefetches(),
                        speculativeLoadingConfig.getPrefetchTTLSeconds(),
                        speculativeLoadingConfig.getMaxPrerenders()));
    }

    private PrefetchOperationCallback createOperationCallback(
            /* PrefetchOperationCallback */ InvocationHandler callback) {
        PrefetchOperationCallbackBoundaryInterface operationCallback =
                BoundaryInterfaceReflectionUtil.castToSuppLibClass(
                        PrefetchOperationCallbackBoundaryInterface.class, callback);
        return new PrefetchOperationCallback() {
            @Override
            public void onSuccess() {
                operationCallback.onSuccess();
            }

            @Override
            public void onError(
                    @PrefetchOperationStatusCode int errorCode,
                    String message,
                    int networkErrorCode) {
                mapFailure(operationCallback, errorCode, message, networkErrorCode);
            }
        };
    }

    private void mapFailure(
            PrefetchOperationCallbackBoundaryInterface callback,
            @PrefetchOperationStatusCode int errorCode,
            String message,
            int networkErrorCode) {
        int type =
                switch (errorCode) {
                    case PrefetchOperationStatusCode
                            .SERVER_FAILURE -> PrefetchOperationCallbackBoundaryInterface
                            .PrefetchExceptionTypeBoundaryInterface.NETWORK;
                    case PrefetchOperationStatusCode
                            .DUPLICATE_REQUEST -> PrefetchOperationCallbackBoundaryInterface
                            .PrefetchExceptionTypeBoundaryInterface.DUPLICATE;
                    default -> PrefetchOperationCallbackBoundaryInterface
                            .PrefetchExceptionTypeBoundaryInterface.GENERIC;
                };
        callback.onFailure(type, message, networkErrorCode);
    }
}
