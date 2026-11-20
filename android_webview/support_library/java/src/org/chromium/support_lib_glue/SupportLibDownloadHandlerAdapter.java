// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import android.webkit.WebView;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwDownloadHandler;
import org.chromium.android_webview.AwWebDownload;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.TraceEvent;
import org.chromium.build.annotations.NullMarked;
import org.chromium.support_lib_boundary.DownloadHandlerBoundaryInterface;
import org.chromium.support_lib_boundary.util.BoundaryInterfaceReflectionUtil;

import java.lang.reflect.InvocationHandler;
import java.util.concurrent.Executor;

/** Support library glue AwDownloadHandler, which passes each download to an AndroidX handler. */
@Lifetime.WebView
@NullMarked
class SupportLibDownloadHandlerAdapter implements AwDownloadHandler {
    // The AwContents this handler is set on, which owns this object.
    private final AwContents mAwContents;
    private final Executor mExecutor;
    private final DownloadHandlerBoundaryInterface mImpl;

    SupportLibDownloadHandlerAdapter(
            AwContents awContents,
            Executor executor,
            /* DownloadHandler */ InvocationHandler handler) {
        mAwContents = awContents;
        mExecutor = executor;
        mImpl =
                BoundaryInterfaceReflectionUtil.castToSuppLibClass(
                        DownloadHandlerBoundaryInterface.class, handler);
    }

    @Override
    public void onDownloadRequested(AwWebDownload download) {
        // Looked up for each download, since the AwContents can be moved to another WebView.
        WebView webView = (WebView) mAwContents.getPrimaryContainerView();
        /* WebDownload */ InvocationHandler webDownload =
                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                        new SupportLibWebDownloadAdapter(download));
        mExecutor.execute(() -> callHandler(webView, webDownload));
    }

    private void callHandler(WebView webView, /* WebDownload */ InvocationHandler download) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICallback.AndroidX.ON_DOWNLOAD_REQUESTED")) {
            mImpl.onDownloadRequested(webView, download);
        }
    }
}
