// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.content.Context;
import android.webkit.WebView;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContents.DependencyFactory;
import org.chromium.android_webview.AwContents.InternalAccessDelegate;
import org.chromium.android_webview.gfx.AwDrawFnImpl.DrawFnAccess;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Represents underlying Chromium web contents state that can survive moving across WebViews. */
@NullMarked
public class WebContent {
    @Nullable private AwContents mAwContents;
    @Nullable private WebViewChromium mCurrentWebViewChromium;
    private boolean mIsDestroyed;

    public boolean isInitialized() {
        return mAwContents != null;
    }

    public AwContents adopt(
            WebViewChromium webViewChromium,
            AwBrowserContext browserContext,
            WebView webView,
            Context context,
            InternalAccessDelegate internalAccessAdapter,
            DrawFnAccess drawFnAccess,
            AwContents.AwContentsClientFactory clientFactory,
            DependencyFactory dependencyFactory) {
        if (mIsDestroyed) {
            throw new IllegalStateException(
                    "Cannot adopt a WebContent instance after destroy() has been called.");
        }

        assert mCurrentWebViewChromium != webViewChromium
                : "Cannot adopt a WebContent into the same WebView twice.";

        if (mCurrentWebViewChromium != null) {
            mCurrentWebViewChromium.detachForTransfer();
        }
        mCurrentWebViewChromium = webViewChromium;

        if (mAwContents == null) {
            mAwContents =
                    new AwContents(
                            browserContext,
                            webView,
                            context,
                            internalAccessAdapter,
                            drawFnAccess,
                            clientFactory,
                            webViewChromium::initSettings,
                            dependencyFactory);
        } else {
            webViewChromium.initSettings(mAwContents.getSettings());
            mAwContents.adopt(webView, internalAccessAdapter);
        }
        return mAwContents;
    }

    public void destroy() {
        if (mIsDestroyed) return;
        mIsDestroyed = true;
        if (mAwContents != null) {
            mAwContents.destroy();
            mAwContents = null;
        }
    }
}
