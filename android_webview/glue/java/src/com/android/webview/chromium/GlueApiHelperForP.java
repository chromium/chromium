// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.os.Build;
import android.webkit.TracingController;
import android.webkit.WebViewDelegate;

import androidx.annotation.RequiresApi;

/**
 * Utility class to use new APIs that were added in P (API level 28). These need to exist in a
 * separate class so that Android framework can successfully verify glue layer classes without
 * encountering the new APIs. Note that GlueApiHelper is only for APIs that cannot go to ApiHelper
 * in base/, for reasons such as using system APIs or instantiating an adapter class that is
 * specific to glue layer.
 */
@RequiresApi(Build.VERSION_CODES.P)
public final class GlueApiHelperForP {
    private GlueApiHelperForP() {}

    /**
     * See {@link
     * TracingControllerAdapter#TracingControllerAdapter(WebViewChromiumFactoryProvider,
     * AwTracingController)}, which was added in P.
     */
    public static TracingController createTracingControllerAdapter(
            WebViewChromiumFactoryProvider provider, WebViewChromiumAwInit awInit) {
        return new TracingControllerAdapter(
                new SharedTracingControllerAdapter(
                        awInit.getRunQueue(), awInit.getAwTracingController()));
    }

    public static String getDataDirectorySuffix(WebViewDelegate webViewDelegate) {
        return webViewDelegate.getDataDirectorySuffix();
    }
}
