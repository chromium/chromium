// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.annotation.TargetApi;
import android.os.Build;
import android.webkit.RenderProcessGoneDetail;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.webkit.WebViewDelegate;

import org.chromium.android_webview.AwRenderProcessGoneDetail;
import org.chromium.base.annotations.VerifiesOnO;

/**
 * Utility class to use new APIs that were added in O (API level 26). These need to exist in a
 * separate class so that Android framework can successfully verify glue layer classes without
 * encountering the new APIs. Note that GlueApiHelper is only for APIs that cannot go to ApiHelper
 * in base/, for reasons such as using system APIs or instantiating an adapter class that is
 * specific to glue layer.
 */
@VerifiesOnO
@TargetApi(Build.VERSION_CODES.O)
public final class GlueApiHelperForO {
    private GlueApiHelperForO() {}

    /**
     * See {@link WebViewClient#onRenderProcessGone(WebView, RenderProcessGoneDetail)}, which was
     * added in O.
     *
     * Note that we are calling into AwRenderProcessGoneDetail so leaving it here. Potentially,
     * we might hide RenderProcessGoneDetail's constructor.
     */
    public static boolean onRenderProcessGone(
            WebViewClient webViewClient, WebView webView, AwRenderProcessGoneDetail detail) {
        return webViewClient.onRenderProcessGone(webView, new RenderProcessGoneDetail() {
            @Override
            public boolean didCrash() {
                return detail.didCrash();
            }

            @Override
            public int rendererPriorityAtExit() {
                return detail.rendererPriority();
            }
        });
    }

    public static boolean isMultiProcessEnabled(WebViewDelegate webViewDelegate) {
        return webViewDelegate.isMultiProcessEnabled();
    }
}
