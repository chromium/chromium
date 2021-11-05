// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.Intent;
import android.graphics.Canvas;
import android.os.Build;
import android.os.UserManager;
import android.webkit.ServiceWorkerController;
import android.webkit.WebView;
import android.webkit.WebViewDelegate;

import org.chromium.base.annotations.VerifiesOnN;

/**
 * Utility class to use new APIs that were added in N (API level 24). These need to exist in a
 * separate class so that Android framework can successfully verify glue layer classes without
 * encountering the new APIs. Note that GlueApiHelper is only for APIs that cannot go to ApiHelper
 * in base/, for reasons such as using system APIs or instantiating an adapter class that is
 * specific to glue layer.
 */
@VerifiesOnN
@TargetApi(Build.VERSION_CODES.N)
public final class GlueApiHelperForN {
    private GlueApiHelperForN() {}

    /**
     * See {@link
     * ServiceWorkerControllerAdapter#ServiceWorkerControllerAdapter(AwServiceWorkerController)},
     * which was added in N.
     */
    public static ServiceWorkerController createServiceWorkerControllerAdapter(
            WebViewChromiumAwInit awInit) {
        return new ServiceWorkerControllerAdapter(awInit.getServiceWorkerController());
    }

    /**
     * See {@link Context#isDeviceProtectedStorage()}.
     */
    public static boolean isDeviceProtectedStorage(Context context) {
        return context.isDeviceProtectedStorage();
    }

    /**
     * See {@link UserManager#isUserUnlocked()}.
     */
    public static boolean isUserUnlocked(Context context) {
        return context.getSystemService(UserManager.class).isUserUnlocked();
    }

    public static Context createCredentialProtectedStorageContext(Context context) {
        return context.createCredentialProtectedStorageContext();
    }

    public static void callDrawGlFunction(WebViewDelegate webViewDelegate, Canvas canvas,
            long nativeDrawGlFunctor, Runnable releasedCallback) {
        webViewDelegate.callDrawGlFunction(canvas, nativeDrawGlFunctor, releasedCallback);
    }

    public static void super_startActivityForResult(
            WebView.PrivateAccess webViewPrivate, Intent intent, int requestCode) {
        webViewPrivate.super_startActivityForResult(intent, requestCode);
    }
}
