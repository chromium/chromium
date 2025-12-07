// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import android.net.Uri;
import android.os.Bundle;
import android.os.CancellationSignal;
import android.webkit.ValueCallback;
import android.webkit.WebChromeClient;
import android.webkit.WebViewClient;

import org.jspecify.annotations.NullMarked;
import org.jspecify.annotations.Nullable;

import java.lang.reflect.InvocationHandler;
import java.util.concurrent.Executor;

@NullMarked
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

    @Nullable WebChromeClient getWebChromeClient();

    /* WebViewRenderer */ InvocationHandler getWebViewRenderer();

    /* WebViewRendererClient */ @Nullable InvocationHandler getWebViewRendererClient();

    void setWebViewRendererClient(
            /* WebViewRendererClient */ @Nullable InvocationHandler webViewRendererClient);

    void setProfile(String profileName);

    void setAudioMuted(boolean muted);

    boolean isAudioMuted();

    /* Profile */ InvocationHandler getProfile();

    void prerenderUrl(
            String url,
            @Nullable CancellationSignal cancellationSignal,
            Executor callbackExecutor,
            ValueCallback<Void> activationCallback,
            ValueCallback<Throwable> errorCallback);

    void prerenderUrl(
            String url,
            @Nullable CancellationSignal cancellationSignal,
            Executor callbackExecutor,
            /* SpeculativeLoadingParameters */ InvocationHandler speculativeLoadingParameters,
            ValueCallback<Void> activationCallback,
            ValueCallback<Throwable> errorCallback);

    void saveState(Bundle outState, int maxSize, boolean includeForwardState);

    void addWebViewNavigationListener(
            Executor executor, /* WebViewNavigationListener */ InvocationHandler listener);

    void removeWebViewNavigationListener(
            /* WebViewNavigationListener */ InvocationHandler listener);

    /* WebViewNavigationClient */ @Nullable InvocationHandler getWebViewNavigationClient();

    void setWebViewNavigationClient(
            /* WebViewNavigationClient */ @Nullable InvocationHandler webViewNavigationClient);
}
