// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.webkit.CookieManager;
import android.webkit.ValueCallback;
import android.webkit.WebView;

import org.chromium.android_webview.AwCookieManager;
import org.chromium.android_webview.WebAddressParser;
import org.chromium.base.Log;

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
        mChromeCookieManager.setAcceptCookie(accept);
    }

    @Override
    public synchronized boolean acceptCookie() {
        return mChromeCookieManager.acceptCookie();
    }

    @Override
    public synchronized void setAcceptThirdPartyCookies(WebView webView, boolean accept) {
        webView.getSettings().setAcceptThirdPartyCookies(accept);
    }

    @Override
    public synchronized boolean acceptThirdPartyCookies(WebView webView) {
        return webView.getSettings().getAcceptThirdPartyCookies();
    }

    @Override
    public void setCookie(String url, String value) {
        if (value == null) {
            Log.e(TAG, "Not setting cookie with null value for URL: %s", url);
            return;
        }

        try {
            mChromeCookieManager.setCookie(fixupUrl(url), value);
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
            mChromeCookieManager.setCookie(
                    fixupUrl(url), value, CallbackConverter.fromValueCallback(callback));
        } catch (URISyntaxException e) {
            Log.e(TAG, "Not setting cookie due to error parsing URL: %s", url, e);
        }
    }

    @Override
    public String getCookie(String url) {
        try {
            return mChromeCookieManager.getCookie(fixupUrl(url));
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
        mChromeCookieManager.removeSessionCookies();
    }

    @Override
    public void removeSessionCookies(final ValueCallback<Boolean> callback) {
        mChromeCookieManager.removeSessionCookies(CallbackConverter.fromValueCallback(callback));
    }

    @Override
    public void removeAllCookie() {
        mChromeCookieManager.removeAllCookies();
    }

    @Override
    public void removeAllCookies(final ValueCallback<Boolean> callback) {
        mChromeCookieManager.removeAllCookies(CallbackConverter.fromValueCallback(callback));
    }

    @Override
    public synchronized boolean hasCookies() {
        return mChromeCookieManager.hasCookies();
    }

    @Override
    public synchronized boolean hasCookies(boolean privateBrowsing) {
        return mChromeCookieManager.hasCookies();
    }

    @Override
    public void removeExpiredCookie() {
        mChromeCookieManager.removeExpiredCookies();
    }

    @Override
    public void flush() {
        mChromeCookieManager.flushCookieStore();
    }

    @Override
    protected boolean allowFileSchemeCookiesImpl() {
        return mChromeCookieManager.allowFileSchemeCookies();
    }

    @Override
    protected void setAcceptFileSchemeCookiesImpl(boolean accept) {
        mChromeCookieManager.setAcceptFileSchemeCookies(accept);
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
