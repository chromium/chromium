// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import static org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.recordApiCall;

import org.chromium.android_webview.AwPage;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.TraceEvent;
import org.chromium.support_lib_boundary.WebViewPageBoundaryInterface;
import org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.ApiCall;

import java.util.concurrent.Callable;

/**
 * Adapter between WebViewPageBoundaryInterface and Page.
 *
 * <p>Once created, instances are kept alive by the peer Page.
 */
@Lifetime.Temporary
class SupportLibWebViewPageAdapter implements WebViewPageBoundaryInterface {
    private final AwPage mPage;

    SupportLibWebViewPageAdapter(AwPage page) {
        mPage = page;
    }

    @Override
    public boolean isPrerendering() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.PAGE_IS_PRERENDERING")) {
            recordApiCall(ApiCall.PAGE_IS_PRERENDERING);
            return mPage.isPrerendering();
        }
    }

    @Override
    public Object getOrCreatePeer(Callable<Object> creationCallable) {
        return mPage.getOrCreateSupportLibObject(creationCallable);
    }
}
