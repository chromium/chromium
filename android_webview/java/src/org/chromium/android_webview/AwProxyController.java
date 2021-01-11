// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import androidx.annotation.IntDef;

import org.chromium.base.annotations.CalledByNativeUnchecked;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;

import java.util.concurrent.Executor;

/**
 * Manages proxy override functionality in WebView.
 */
@JNINamespace("android_webview")
public class AwProxyController {
    /**
     * Represents the scheme used in proxy rules.
     * These values are persisted to logs. Entries should not be renumbered
     * or reordered and numeric values should never be reused.
     */
    @IntDef({ProxySchemeType.HTTP, ProxySchemeType.HTTPS, ProxySchemeType.ALL})
    private @interface ProxySchemeType {
        int HTTP = 0;
        int HTTPS = 1;
        int ALL = 2;
        int NUM_ENTRIES = 3;
    }

    /**
     * Represents the type of proxy url.
     * These values are persisted to logs. Entries should not be renumbered
     * or reordered and numeric values should never be reused.
     */
    @IntDef({ProxyUrlType.HTTP, ProxyUrlType.HTTPS, ProxyUrlType.DIRECT})
    private @interface ProxyUrlType {
        int HTTP = 0;
        int HTTPS = 1;
        int DIRECT = 2;
        int NUM_ENTRIES = 3;
    }

    public AwProxyController() {}

    public void setProxyOverride(String[][] proxyRules, String[] bypassRules, Runnable listener,
            Executor executor, boolean reverseBypass) {
        int length = (proxyRules == null ? 0 : proxyRules.length);
        String[] urlSchemes = new String[length];
        String[] proxyUrls = new String[length];
        boolean schemeHttp = false;
        boolean schemeHttps = false;
        boolean urlHttp = false;
        boolean urlHttps = false;
        boolean urlDirect = false;
        for (int i = 0; i < length; i++) {
            // URL schemes
            if (proxyRules[i][0] == null) {
                urlSchemes[i] = "*";
            } else {
                urlSchemes[i] = proxyRules[i][0];
            }
            // proxy URLs
            proxyUrls[i] = proxyRules[i][1];
            if (proxyUrls[i] == null) {
                throw new IllegalArgumentException("Proxy rule " + i + " has a null url");
            }
            // Check schemes for UMA
            if (proxyRules[i][0].equals("http")) {
                schemeHttp = true;
            } else if (proxyRules[i][0].equals("https")) {
                schemeHttps = true;
            } else {
                schemeHttp = true;
                schemeHttps = true;
            }
            // Check URLs for UMA
            if (proxyUrls[i].startsWith("http://")) {
                urlHttp = true;
            } else if (proxyUrls[i].startsWith("https://")) {
                urlHttps = true;
            } else if (proxyUrls[i].startsWith("direct://")) {
                urlDirect = true;
            }
        }
        length = (bypassRules == null ? 0 : bypassRules.length);
        for (int i = 0; i < length; i++) {
            if (bypassRules[i] == null) {
                throw new IllegalArgumentException("Bypass rule " + i + " is null");
            }
        }
        if (executor == null) {
            throw new IllegalArgumentException("Executor must not be null");
        }

        String result = AwProxyControllerJni.get().setProxyOverride(AwProxyController.this,
                urlSchemes, proxyUrls, bypassRules, listener, executor, reverseBypass);
        if (!result.isEmpty()) {
            throw new IllegalArgumentException(result);
        }

        // In case operation is successful, log UMA data on SetProxyOverride
        // Proxy scheme filter
        if (schemeHttp && schemeHttps) {
            recordProxySchemeType(ProxySchemeType.ALL);
        } else if (schemeHttp) {
            recordProxySchemeType(ProxySchemeType.HTTP);
        } else if (schemeHttps) {
            recordProxySchemeType(ProxySchemeType.HTTPS);
        }
        // Proxy url type
        if (urlHttp) {
            recordProxyUrlType(ProxyUrlType.HTTP);
        }
        if (urlHttps) {
            recordProxyUrlType(ProxyUrlType.HTTPS);
        }
        if (urlDirect) {
            recordProxyUrlType(ProxyUrlType.DIRECT);
        }
        // Bypass rules
        RecordHistogram.recordBooleanHistogram("Android.WebView.SetProxyOverride.BypassRules",
                bypassRules == null || bypassRules.length == 0 ? false : true);
    }

    private static void recordProxySchemeType(@ProxySchemeType int proxySchemeType) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.WebView.SetProxyOverride.ProxySchemeFilterType", proxySchemeType,
                ProxySchemeType.NUM_ENTRIES);
    }

    private static void recordProxyUrlType(@ProxyUrlType int proxyUrlType) {
        RecordHistogram.recordEnumeratedHistogram("Android.WebView.SetProxyOverride.ProxyUrlType",
                proxyUrlType, ProxyUrlType.NUM_ENTRIES);
    }

    public void clearProxyOverride(Runnable listener, Executor executor) {
        if (executor == null) {
            throw new IllegalArgumentException("Executor must not be null");
        }

        AwProxyControllerJni.get().clearProxyOverride(AwProxyController.this, listener, executor);
        // Log UMA data on ClearProxyOverride
        RecordHistogram.recordBooleanHistogram("Android.WebView.ClearProxyOverride", true);
    }

    @CalledByNativeUnchecked
    private void proxyOverrideChanged(Runnable listener, Executor executor) {
        if (listener == null) return;
        executor.execute(listener);
    }

    @NativeMethods
    interface Natives {
        String setProxyOverride(AwProxyController caller, String[] urlSchemes, String[] proxyUrls,
                String[] bypassRules, Runnable listener, Executor executor, boolean reverseBypass);
        void clearProxyOverride(AwProxyController caller, Runnable listener, Executor executor);
    }
}
