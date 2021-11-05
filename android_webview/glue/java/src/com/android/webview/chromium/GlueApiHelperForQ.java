// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.annotation.TargetApi;
import android.os.Build;
import android.webkit.WebViewRenderProcess;
import android.webkit.WebViewRenderProcessClient;

import org.chromium.android_webview.AwRenderProcess;
import org.chromium.base.annotations.VerifiesOnQ;

import java.util.concurrent.Executor;

/**
 * Utility class to use new APIs that were added in Q (API level 29). These need to exist in a
 * separate class so that Android framework can successfully verify glue layer classes without
 * encountering the new APIs. Note that GlueApiHelper is only for APIs that cannot go to ApiHelper
 * in base/, for reasons such as using system APIs or instantiating an adapter class that is
 * specific to glue layer.
 */
@VerifiesOnQ
@TargetApi(Build.VERSION_CODES.Q)
public final class GlueApiHelperForQ {
    private GlueApiHelperForQ() {}

    /** @see {@link WebView#getWebViewRenderProcess()} */
    public static WebViewRenderProcess getWebViewRenderProcess(AwRenderProcess awRenderProcess) {
        return WebViewRenderProcessAdapter.getInstanceFor(awRenderProcess);
    }

    /**
     * @see {@link WebView#setWebViewRenderProcessClient(Executor,
     * WebViewRenderProcessClient)}
     */
    public static void setWebViewRenderProcessClient(SharedWebViewChromium sharedWebViewChromium,
            Executor executor, WebViewRenderProcessClient client) {
        sharedWebViewChromium.setWebViewRendererClientAdapter(
                new WebViewRenderProcessClientAdapter(executor, client));
    }

    /** @see {@link WebView#getWebViewRenderProcessClient() */
    public static WebViewRenderProcessClient getWebViewRenderProcessClient(
            SharedWebViewRendererClientAdapter adapter) {
        return ((WebViewRenderProcessClientAdapter) adapter).getWebViewRenderProcessClient();
    }
}
