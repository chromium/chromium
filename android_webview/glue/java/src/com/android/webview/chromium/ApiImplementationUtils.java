// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.graphics.Bitmap;
import android.webkit.WebChromeClient;
import android.webkit.WebView;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.reflect.Method;

@NullMarked
public final class ApiImplementationUtils {

    private static final String TAG = "ApiImplUtils";

    private ApiImplementationUtils() {}

    public static boolean isOnReceivedIconOverridden(@Nullable WebChromeClient client) {
        if (client == null) {
            return false;
        }

        try {
            Class[] onReceivedIconParameters = {WebView.class, Bitmap.class};
            Method implementedMethod =
                    client.getClass().getMethod("onReceivedIcon", onReceivedIconParameters);
            // Checks whether the onReceivedIcon method's declaring class is the same class as
            // WebChromeClient - if not, it has been overridden by another class
            return !WebChromeClient.class.equals(implementedMethod.getDeclaringClass());
        } catch (NoSuchMethodException e) {
            Log.w(TAG, "No onReceivedIcon method found in set WebChromeClient");
            // return true, otherwise worst case scenario onReceivedIcon might
            // not be called from the native side as it will skip copying the favicon
            // over the JNI.
            return true;
        }
    }
}
