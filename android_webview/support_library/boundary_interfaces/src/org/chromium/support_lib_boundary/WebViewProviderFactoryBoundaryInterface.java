// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import android.webkit.WebView;

import org.jspecify.annotations.NullMarked;

import java.lang.reflect.InvocationHandler;

/** Boundary interface for WebView globals and singletons. */
@NullMarked
public interface WebViewProviderFactoryBoundaryInterface {

    // LINT.IfChange(MultiCookieKeys)
    String MULTI_COOKIE_HEADER_NAME = "\0Set-Cookie-Multivalue\0";
    String MULTI_COOKIE_VALUE_SEPARATOR = "\0";

    // LINT.ThenChange(/components/embedder_support/android/util/web_resource_response.cc:MultiCookieKeys)

    /* WebViewBuilderBoundaryInterface */ InvocationHandler getWebViewBuilder();

    /* SupportLibraryWebViewChromium */ InvocationHandler createWebView(WebView webview);

    /* SupportLibWebkitToCompatConverter */ InvocationHandler getWebkitToCompatConverter();

    /* StaticsAdapter */ InvocationHandler getStatics();

    String[] getSupportedFeatures();

    /* SupportLibraryServiceWorkerController */ InvocationHandler getServiceWorkerController();

    /* SupportLibraryTracingController */ InvocationHandler getTracingController();

    /* SupportLibraryProxyController */ InvocationHandler getProxyController();

    /* DropDataContentProviderBoundaryInterface*/ InvocationHandler getDropDataProvider();

    /* ProfileStoreBoundaryInterface */ InvocationHandler getProfileStore();

    void startUpWebView(
            /* WebViewStartUpConfig */ InvocationHandler config,
            /* WebViewStartUpCallback */ InvocationHandler callback);
}
