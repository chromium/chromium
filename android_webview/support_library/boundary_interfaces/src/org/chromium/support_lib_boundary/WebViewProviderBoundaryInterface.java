// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import android.net.Uri;
import android.webkit.WebChromeClient;
import android.webkit.WebViewClient;

import java.lang.reflect.InvocationHandler;

/**
 */
public interface WebViewProviderBoundaryInterface {
    void insertVisualStateCallback(
            long requestId, /* VisualStateCallback */ InvocationHandler callback);

    /* WebMessagePort */ InvocationHandler[] createWebMessageChannel();

    void postMessageToMainFrame(/* WebMessage */ InvocationHandler message, Uri targetOrigin);

    void addWebMessageListener(
            String jsObjectName,
            String[] allowedOriginRules,
            /* WebMessageListener */ InvocationHandler listener);

    void removeWebMessageListener(String jsObjectName);

    /* ScriptHandler */ InvocationHandler addDocumentStartJavaScript(
            String script, String[] allowedOriginRules);

    WebViewClient getWebViewClient();

    WebChromeClient getWebChromeClient();

    /* WebViewRenderer */ InvocationHandler getWebViewRenderer();

    /* WebViewRendererClient */ InvocationHandler getWebViewRendererClient();

    void setWebViewRendererClient(
            /* WebViewRendererClient */ InvocationHandler webViewRendererClient);

    void setProfile(String profileName);

    void setAudioMuted(boolean muted);

    boolean isAudioMuted();

    /* Profile */ InvocationHandler getProfile();
}
