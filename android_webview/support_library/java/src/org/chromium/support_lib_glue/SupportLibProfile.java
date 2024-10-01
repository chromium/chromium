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

import com.android.webview.chromium.NoVarySearchData;
import com.android.webview.chromium.PrefetchParams;
import com.android.webview.chromium.Profile;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.support_lib_boundary.NoVarySearchDataBoundaryInterface;
import org.chromium.support_lib_boundary.PrefetchParamsBoundaryInterface;
import org.chromium.support_lib_boundary.ProfileBoundaryInterface;
import org.chromium.support_lib_boundary.util.BoundaryInterfaceReflectionUtil;
import org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.ApiCall;

import java.lang.reflect.InvocationHandler;

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
            ValueCallback</* PrefetchOperationResultBoundaryInterface */ InvocationHandler>
                    resultCallback) {
        recordApiCall(ApiCall.PREFETCH_URL);
        mProfileImpl.prefetchUrl(
                url,
                null,
                value ->
                        resultCallback.onReceiveValue(
                                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                        new SupportLibPrefetchOperationResult(value))));
    }

    @Override
    public void prefetchUrl(
            String url,
            /* PrefetchParamsBoundaryInterface */ InvocationHandler callbackInvocation,
            ValueCallback</* PrefetchOperationResultBoundaryInterface */ InvocationHandler>
                    resultCallback) {
        recordApiCall(ApiCall.PREFETCH_URL_WITH_PARAMS);
        PrefetchParamsBoundaryInterface prefetchParams =
                BoundaryInterfaceReflectionUtil.castToSuppLibClass(
                        PrefetchParamsBoundaryInterface.class, callbackInvocation);

        NoVarySearchDataBoundaryInterface noVarySearchData =
                BoundaryInterfaceReflectionUtil.castToSuppLibClass(
                        NoVarySearchDataBoundaryInterface.class,
                        prefetchParams.getNoVarySearchData());

        mProfileImpl.prefetchUrl(
                url,
                new PrefetchParams(
                        prefetchParams.getAdditionalHeaders(),
                        mapNoVarySearchData(noVarySearchData)),
                value ->
                        resultCallback.onReceiveValue(
                                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                        new SupportLibPrefetchOperationResult(value))));
    }

    private NoVarySearchData mapNoVarySearchData(
            NoVarySearchDataBoundaryInterface noVarySearchData) {
        if (noVarySearchData == null) return null;
        return new NoVarySearchData(
                noVarySearchData.getVaryOnKeyOrder(),
                noVarySearchData.getIgnoreDifferencesInParameters(),
                noVarySearchData.getIgnoredQueryParameters(),
                noVarySearchData.getConsideredQueryParameters());
    }

    @Override
    public void cancelPrefetch(
            String url,
            ValueCallback</* PrefetchOperationResultBoundaryInterface */ InvocationHandler>
                    resultCallback) {
        recordApiCall(ApiCall.CANCEL_PREFETCH);
        mProfileImpl.cancelPrefetch(
                url,
                value ->
                        resultCallback.onReceiveValue(
                                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                        new SupportLibPrefetchOperationResult(value))));
    }

    @Override
    public void clearPrefetch(
            String url,
            ValueCallback</* PrefetchOperationResultBoundaryInterface */ InvocationHandler>
                    resultCallback) {
        recordApiCall(ApiCall.CLEAR_PREFETCH);
        mProfileImpl.clearPrefetch(
                url,
                value ->
                        resultCallback.onReceiveValue(
                                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                        new SupportLibPrefetchOperationResult(value))));
    }
}
