// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import static org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.recordApiCall;

import org.chromium.android_webview.AwRenderProcess;
import org.chromium.android_webview.AwSupportLibIsomorphic;
import org.chromium.support_lib_boundary.WebViewRendererBoundaryInterface;
import org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.ApiCall;

/**
 * Adapter between WebViewRendererBoundaryInterface and AwRenderProcess.
 */
class SupportLibWebViewRendererAdapter
        extends IsomorphicAdapter implements WebViewRendererBoundaryInterface {
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
        recordApiCall(ApiCall.WEBVIEW_RENDERER_TERMINATE);
        return mRenderer.terminate();
    }
}
