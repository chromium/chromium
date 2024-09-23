// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.webkit.GeolocationPermissions;
import android.webkit.ValueCallback;

import com.android.webview.chromium.WebViewChromium.ApiCall;

import org.chromium.android_webview.AwGeolocationPermissions;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;

import java.util.Set;

/**
 * Chromium implementation of GeolocationPermissions -- forwards calls to the chromium internal
 * implementation.
 */
final class GeolocationPermissionsAdapter extends GeolocationPermissions {

    private final WebViewChromiumFactoryProvider mFactory;
    private final AwGeolocationPermissions mChromeGeolocationPermissions;

    public GeolocationPermissionsAdapter(
            WebViewChromiumFactoryProvider factory,
            AwGeolocationPermissions chromeGeolocationPermissions) {
        mFactory = factory;
        mChromeGeolocationPermissions = chromeGeolocationPermissions;
    }

    @Override
    public void allow(final String origin) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.GEOLOCATION_PERMISSIONS_ALLOW")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.GEOLOCATION_PERMISSIONS_ALLOW);
            if (checkNeedsPost()) {
                mFactory.addTask(() -> mChromeGeolocationPermissions.allow(origin));
                return;
            }
            mChromeGeolocationPermissions.allow(origin);
        }
    }

    @Override
    public void clear(final String origin) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.GEOLOCATION_PERMISSIONS_CLEAR")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.GEOLOCATION_PERMISSIONS_CLEAR);
            if (checkNeedsPost()) {
                mFactory.addTask(() -> mChromeGeolocationPermissions.clear(origin));
                return;
            }
            mChromeGeolocationPermissions.clear(origin);
        }
    }

    @Override
    public void clearAll() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.GEOLOCATION_PERMISSIONS_CLEAR_ALL")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.GEOLOCATION_PERMISSIONS_CLEAR_ALL);
            if (checkNeedsPost()) {
                mFactory.addTask(mChromeGeolocationPermissions::clearAll);
                return;
            }
            mChromeGeolocationPermissions.clearAll();
        }
    }

    @Override
    public void getAllowed(final String origin, final ValueCallback<Boolean> callback) {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.GEOLOCATION_PERMISSIONS_GET_ALLOWED")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.GEOLOCATION_PERMISSIONS_GET_ALLOWED);
            if (checkNeedsPost()) {
                mFactory.addTask(
                        () ->
                                mChromeGeolocationPermissions.getAllowed(
                                        origin, CallbackConverter.fromValueCallback(callback)));
                return;
            }
            mChromeGeolocationPermissions.getAllowed(
                    origin, CallbackConverter.fromValueCallback(callback));
        }
    }

    @Override
    public void getOrigins(final ValueCallback<Set<String>> callback) {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.GEOLOCATION_PERMISSIONS_GET_ORIGINS")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.GEOLOCATION_PERMISSIONS_GET_ORIGINS);

            if (checkNeedsPost()) {
                mFactory.addTask(
                        () ->
                                mChromeGeolocationPermissions.getOrigins(
                                        CallbackConverter.fromValueCallback(callback)));
                return;
            }
            mChromeGeolocationPermissions.getOrigins(CallbackConverter.fromValueCallback(callback));
        }
    }

    private static boolean checkNeedsPost() {
        // Init is guaranteed to have happened if a GeolocationPermissionsAdapter is created, so do
        // not need to check WebViewChromiumFactoryProvider.hasStarted.
        return !ThreadUtils.runningOnUiThread();
    }
}
