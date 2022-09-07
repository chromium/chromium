// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.webkit.GeolocationPermissions;
import android.webkit.ValueCallback;

import org.chromium.android_webview.AwGeolocationPermissions;
import org.chromium.base.ThreadUtils;

import java.util.Set;

/**
 * Chromium implementation of GeolocationPermissions -- forwards calls to the
 * chromium internal implementation.
 */
final class GeolocationPermissionsAdapter extends GeolocationPermissions {
    private final WebViewChromiumFactoryProvider mFactory;
    private final AwGeolocationPermissions mChromeGeolocationPermissions;

    public GeolocationPermissionsAdapter(WebViewChromiumFactoryProvider factory,
            AwGeolocationPermissions chromeGeolocationPermissions) {
        mFactory = factory;
        mChromeGeolocationPermissions = chromeGeolocationPermissions;
    }

    @Override
    public void allow(final String origin) {
        if (checkNeedsPost()) {
            mFactory.addTask(new Runnable() {
                @Override
                public void run() {
                    mChromeGeolocationPermissions.allow(origin);
                }

            });
            return;
        }
        mChromeGeolocationPermissions.allow(origin);
    }

    @Override
    public void clear(final String origin) {
        if (checkNeedsPost()) {
            mFactory.addTask(new Runnable() {
                @Override
                public void run() {
                    mChromeGeolocationPermissions.clear(origin);
                }

            });
            return;
        }
        mChromeGeolocationPermissions.clear(origin);
    }

    @Override
    public void clearAll() {
        if (checkNeedsPost()) {
            mFactory.addTask(new Runnable() {
                @Override
                public void run() {
                    mChromeGeolocationPermissions.clearAll();
                }

            });
            return;
        }
        mChromeGeolocationPermissions.clearAll();
    }

    @Override
    public void getAllowed(final String origin, final ValueCallback<Boolean> callback) {
        if (checkNeedsPost()) {
            mFactory.addTask(new Runnable() {
                @Override
                public void run() {
                    mChromeGeolocationPermissions.getAllowed(
                            origin, CallbackConverter.fromValueCallback(callback));
                }

            });
            return;
        }
        mChromeGeolocationPermissions.getAllowed(
                origin, CallbackConverter.fromValueCallback(callback));
    }

    @Override
    public void getOrigins(final ValueCallback<Set<String>> callback) {
        if (checkNeedsPost()) {
            mFactory.addTask(new Runnable() {
                @Override
                public void run() {
                    mChromeGeolocationPermissions.getOrigins(
                            CallbackConverter.fromValueCallback(callback));
                }

            });
            return;
        }
        mChromeGeolocationPermissions.getOrigins(CallbackConverter.fromValueCallback(callback));
    }

    private static boolean checkNeedsPost() {
        // Init is guaranteed to have happened if a GeolocationPermissionsAdapter is created, so do
        // not need to check WebViewChromiumFactoryProvider.hasStarted.
        return !ThreadUtils.runningOnUiThread();
    }
}
