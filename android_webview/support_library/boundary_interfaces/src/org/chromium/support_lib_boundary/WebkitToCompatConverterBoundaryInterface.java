// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import android.webkit.WebResourceRequest;
import android.webkit.WebSettings;

import java.lang.reflect.InvocationHandler;

/**
 * Boundary interface for a class used for converting webkit objects into Compat (support library)
 * objects.
 */
public interface WebkitToCompatConverterBoundaryInterface {
    // ====================================================
    // Pre-L classes, these only need conversion methods from webkit -> support library since their
    // support library implementations are all static methods.
    // Webkit -> support library conversions.
    // ====================================================

    /* SupportLibraryWebSettings */ InvocationHandler convertSettings(WebSettings webSettings);

    /* SupportLibraryWebResourceRequest */ InvocationHandler convertWebResourceRequest(
            WebResourceRequest request);

    // ====================================================
    // Post-L classes, these classes need conversion methods both from webkit -> support library and
    // back because they have full functional implementations in the support library (so some APIs
    // will return the support library version of the class).
    //
    // None of the following methods can be called before the API level in which the related webkit
    // class was introduced (e.g. API level 24 for convertServiceWorkerSettings since
    // android.webkit.ServiceWorkerWebSettings was introduced at API level 24).
    //
    // Note that these methods use the 'Object' class to represent webkit classes because these are
    // post-L webkit classes. Whenever we would create a Proxy for the version of
    // WebkitToCompatConverterBoundaryInterface using actual webkit classes on an L device the Proxy
    // class would fail to look up post-L webkit classes and thus cause a crash, see
    // http://crbug.com/831554.
    // ====================================================

    // ServiceWorkerWebSettings
    /* SupportLibServiceWorkerSettings */ InvocationHandler convertServiceWorkerSettings(
            /* ServiceWorkerWebSettings */ Object serviceWorkerWebSettings);

    /* ServiceWorkerWebSettings */ Object convertServiceWorkerSettings(
            /* SupportLibServiceWorkerSettings */ InvocationHandler serviceWorkerSettings);

    // WebResourceError
    /* SupportLibWebResourceError */ InvocationHandler convertWebResourceError(
            /* WebResourceError */ Object webResourceError);

    /* WebResourceError */ Object convertWebResourceError(
            /* SupportLibWebResourceError */ InvocationHandler webResourceError);

    // SafeBrowsingResponse
    /* SupportLibSafeBrowsingResponse */ InvocationHandler convertSafeBrowsingResponse(
            /* SafeBrowsingResponse */ Object safeBrowsingResponse);

    /* SafeBrowsingResponse */ Object convertSafeBrowsingResponse(
            /* SupportLibSafeBrowsingResponse */ InvocationHandler safeBrowsingResponse);

    // WebMessagePort
    /* SupportLibWebMessagePort */ InvocationHandler convertWebMessagePort(
            /* WebMessagePort */ Object webMessagePort);

    /* WebMessagePort */ Object convertWebMessagePort(
            /* SupportLibWebMessagePort */ InvocationHandler webMessagePort);

    // CookieManager
    /* SupportLibWebViewCookieManager */ InvocationHandler convertCookieManager(
            Object cookieManager);
}
