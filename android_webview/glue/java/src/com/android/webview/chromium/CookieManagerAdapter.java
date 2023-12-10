// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.webkit.CookieManager;
import android.webkit.ValueCallback;
import android.webkit.WebView;

import com.android.webview.chromium.WebViewChromium.ApiCall;

import org.chromium.android_webview.AwCookieManager;
import org.chromium.android_webview.WebAddressParser;
import org.chromium.base.Log;
import org.chromium.base.TraceEvent;

import java.net.URISyntaxException;

/**
 * Chromium implementation of CookieManager -- forwards calls to the
 * chromium internal implementation.
 */
@SuppressWarnings({"deprecation", "NoSynchronizedMethodCheck"})
public class CookieManagerAdapter extends CookieManager {
    private static final String TAG = "CookieManager";

    AwCookieManager mChromeCookieManager;

    public CookieManagerAdapter(AwCookieManager chromeCookieManager) {
        mChromeCookieManager = chromeCookieManager;
    }

    public AwCookieManager getCookieManager() {
        return mChromeCookieManager;
    }

    @Override
    public synchronized void setAcceptCookie(boolean accept) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.COOKIE_MANAGER_SET_ACCEPT_COOKIE")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.COOKIE_MANAGER_SET_ACCEPT_COOKIE);
            mChromeCookieManager.setAcceptCookie(accept);
        }
    }

    @Override
    public synchronized boolean acceptCookie() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.COOKIE_MANAGER_ACCEPT_COOKIE")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.COOKIE_MANAGER_ACCEPT_COOKIE);
            return mChromeCookieManager.acceptCookie();
        }
    }

    @Override
    public synchronized void setAcceptThirdPartyCookies(WebView webView, boolean accept) {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.ApiCall.COOKIE_MANAGER_SET_ACCEPT_THIRD_PARTY_COOKIES")) {
            WebViewChromium.recordWebViewApiCall(
                    ApiCall.COOKIE_MANAGER_SET_ACCEPT_THIRD_PARTY_COOKIES);
            webView.getSettings().setAcceptThirdPartyCookies(accept);
        }
    }

    @Override
    public synchronized boolean acceptThirdPartyCookies(WebView webView) {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.COOKIE_MANAGER_ACCEPT_THIRD_PARTY_COOKIES")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.COOKIE_MANAGER_ACCEPT_THIRD_PARTY_COOKIES);
            return webView.getSettings().getAcceptThirdPartyCookies();
        }
    }

    @Override
    public void setCookie(String url, String value) {
        if (value == null) {
            Log.e(TAG, "Not setting cookie with null value for URL: %s", url);
            return;
        }

        try {
            try (TraceEvent event =
                    TraceEvent.scoped("WebView.APICall.Framework.COOKIE_MANAGER_SET_COOKIE")) {
                WebViewChromium.recordWebViewApiCall(ApiCall.COOKIE_MANAGER_SET_COOKIE);
                mChromeCookieManager.setCookie(fixupUrl(url), value);
            }
        } catch (URISyntaxException e) {
            Log.e(TAG, "Not setting cookie due to error parsing URL: %s", url, e);
        }
    }

    @Override
    public void setCookie(String url, String value, ValueCallback<Boolean> callback) {
        if (value == null) {
            Log.e(TAG, "Not setting cookie with null value for URL: %s", url);
            return;
        }

        try {
            try (TraceEvent event =
                    TraceEvent.scoped("WebView.APICall.Framework.COOKIE_MANAGER_SET_COOKIE")) {
                WebViewChromium.recordWebViewApiCall(ApiCall.COOKIE_MANAGER_SET_COOKIE);
                mChromeCookieManager.setCookie(
                        fixupUrl(url), value, CallbackConverter.fromValueCallback(callback));
            }
        } catch (URISyntaxException e) {
            Log.e(TAG, "Not setting cookie due to error parsing URL: %s", url, e);
        }
    }

    @Override
    public String getCookie(String url) {
        try {
            try (TraceEvent event =
                    TraceEvent.scoped("WebView.APICall.Framework.COOKIE_MANAGER_GET_COOKIE")) {
                WebViewChromium.recordWebViewApiCall(ApiCall.COOKIE_MANAGER_GET_COOKIE);
                return mChromeCookieManager.getCookie(fixupUrl(url));
            }
        } catch (URISyntaxException e) {
            Log.e(TAG, "Unable to get cookies due to error parsing URL: %s", url, e);
            return null;
        }
    }

    @Override
    public String getCookie(String url, boolean privateBrowsing) {
        return getCookie(url);
    }

    @Override
    public void removeSessionCookie() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.COOKIE_MANAGER_REMOVE_SESSION_COOKIE")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.COOKIE_MANAGER_REMOVE_SESSION_COOKIE);
            mChromeCookieManager.removeSessionCookies();
        }
    }

    @Override
    public void removeSessionCookies(final ValueCallback<Boolean> callback) {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.COOKIE_MANAGER_REMOVE_SESSION_COOKIES")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.COOKIE_MANAGER_REMOVE_SESSION_COOKIES);
            mChromeCookieManager.removeSessionCookies(
                    CallbackConverter.fromValueCallback(callback));
        }
    }

    @Override
    public void removeAllCookie() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.COOKIE_MANAGER_REMOVE_ALL_COOKIE")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.COOKIE_MANAGER_REMOVE_ALL_COOKIE);
            mChromeCookieManager.removeAllCookies();
        }
    }

    @Override
    public void removeAllCookies(final ValueCallback<Boolean> callback) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.COOKIE_MANAGER_REMOVE_ALL_COOKIES")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.COOKIE_MANAGER_REMOVE_ALL_COOKIES);
            mChromeCookieManager.removeAllCookies(CallbackConverter.fromValueCallback(callback));
        }
    }

    @Override
    public synchronized boolean hasCookies() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.COOKIE_MANAGER_HAS_COOKIES")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.COOKIE_MANAGER_HAS_COOKIES);
            return mChromeCookieManager.hasCookies();
        }
    }

    @Override
    public synchronized boolean hasCookies(boolean privateBrowsing) {
        return mChromeCookieManager.hasCookies();
    }

    @Override
    public void removeExpiredCookie() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.COOKIE_MANAGER_REMOVE_EXPIRED_COOKIE")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.COOKIE_MANAGER_REMOVE_EXPIRED_COOKIE);
            mChromeCookieManager.removeExpiredCookies();
        }
    }

    @Override
    public void flush() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.COOKIE_MANAGER_FLUSH")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.COOKIE_MANAGER_FLUSH);
            mChromeCookieManager.flushCookieStore();
        }
    }

    @Override
    protected boolean allowFileSchemeCookiesImpl() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.COOKIE_MANAGER_ALLOW_FILE_SCHEME_COOKIES")) {
            WebViewChromium.recordWebViewApiCall(ApiCall.COOKIE_MANAGER_ALLOW_FILE_SCHEME_COOKIES);
            return mChromeCookieManager.allowFileSchemeCookies();
        }
    }

    @Override
    protected void setAcceptFileSchemeCookiesImpl(boolean accept) {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.COOKIE_MANAGER_SET_ACCEPT_FILE_SCHEME_COOKIES")) {
            WebViewChromium.recordWebViewApiCall(
                    ApiCall.COOKIE_MANAGER_SET_ACCEPT_FILE_SCHEME_COOKIES);
            mChromeCookieManager.setAcceptFileSchemeCookies(accept);
        }
    }

    private static String fixupUrl(String url) throws URISyntaxException {
        // WebAddressParser is a copy of the  private API WebAddress in the android framework and a
        // "quirk" of the Classic WebView implementation that allowed embedders to be relaxed about
        // what URLs they passed into the CookieManager, so we do the same normalisation before
        // entering the chromium stack.
        //
        // The implementation of WebAddressParser isn't ideal, we should remove its usage and
        // replace it with UrlFormatter or similar URL parser.
        return new WebAddressParser(url).toString();
    }
}
