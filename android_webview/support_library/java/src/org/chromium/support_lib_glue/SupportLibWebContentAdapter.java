// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import android.content.Context;
import android.webkit.WebView;

import com.android.webview.chromium.WebContentContextWrapper;

import org.chromium.android_webview.AwWebContent;
import org.chromium.base.TraceEvent;
import org.chromium.build.annotations.NullMarked;
import org.chromium.support_lib_boundary.web.WebContentBoundaryInterface;
import org.chromium.support_lib_glue.WebApiCallLogger.WebApiCall;

import java.util.function.Function;

/** Adapter for WebContentBoundaryInterface. */
@NullMarked
class SupportLibWebContentAdapter implements WebContentBoundaryInterface {
    private final AwWebContent mWebContent;

    public SupportLibWebContentAdapter(AwWebContent webContent) {
        mWebContent = webContent;
    }

    public AwWebContent getWebContent() {
        return mWebContent;
    }

    @Override
    public <T extends WebView> T executeViewFactory(
            Context baseContext, Function<Context, T> factory) {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.AndroidX.Web.WEB_CONTENT_EXECUTE_VIEW_FACTORY")) {
            WebApiCallLogger.recordWebApiCall(WebApiCall.WEB_CONTENT_EXECUTE_VIEW_FACTORY);
            WebContentContextWrapper wrapper =
                    new WebContentContextWrapper(baseContext, mWebContent);
            T result = factory.apply(wrapper);
            if (!wrapper.wasUsed()) {
                throw new IllegalStateException("The factory did not construct a new WebView");
            }
            return result;
        }
    }

    @Override
    public void destroy() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.Web.WEB_CONTENT_DESTROY")) {
            WebApiCallLogger.recordWebApiCall(WebApiCall.WEB_CONTENT_DESTROY);
            mWebContent.destroy();
        }
    }
}
