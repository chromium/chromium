// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import static org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.recordApiCall;

import org.chromium.android_webview.AwRenderProcess;
import org.chromium.android_webview.AwSupportLibIsomorphic;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.TraceEvent;
import org.chromium.support_lib_boundary.WebViewRendererBoundaryInterface;
import org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.ApiCall;

/**
 * Adapter between WebViewRendererBoundaryInterface and AwRenderProcess.
 *
 * Once created, instances are kept alive by the peer AwRendererProcess.
 */
@Lifetime.Renderer
class SupportLibWebViewRendererAdapter extends IsomorphicAdapter
        implements WebViewRendererBoundaryInterface {
    private AwRenderProcess mRenderer;

    SupportLibWebViewRendererAdapter(AwRenderProcess renderer) {
        mRenderer = renderer;
    }

    @Override
    AwSupportLibIsomorphic getPeeredObject() {
        return mRenderer;
    }

    @Override
    public boolean terminate() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.WEBVIEW_RENDERER_TERMINATE")) {
            recordApiCall(ApiCall.WEBVIEW_RENDERER_TERMINATE);
            return mRenderer.terminate();
        }
    }
}
