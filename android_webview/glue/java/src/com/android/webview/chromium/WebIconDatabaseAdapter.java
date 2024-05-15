// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.content.ContentResolver;

import com.android.webview.chromium.WebViewChromium.ApiCall;

import org.chromium.android_webview.AwContents;

/** Chromium implementation of WebIconDatabase -- big old no-op (base class is deprecated). */
@SuppressWarnings("deprecation")
final class WebIconDatabaseAdapter extends android.webkit.WebIconDatabase {
    @Override
    public void open(String path) {
        WebViewChromium.recordWebViewApiCall(ApiCall.WEB_ICON_DATABASE_OPEN);
        AwContents.setShouldDownloadFavicons();
    }

    @Override
    public void close() {
        WebViewChromium.recordWebViewApiCall(ApiCall.WEB_ICON_DATABASE_CLOSE);
        // Intentional no-op.
    }

    @Override
    public void removeAllIcons() {
        WebViewChromium.recordWebViewApiCall(ApiCall.WEB_ICON_DATABASE_REMOVE_ALL_ICONS);
        // Intentional no-op: we have no database so nothing to remove.
    }

    @Override
    public void requestIconForPageUrl(String url, IconListener listener) {
        WebViewChromium.recordWebViewApiCall(ApiCall.WEB_ICON_DATABASE_REQUEST_ICON_FOR_PAGE_URL);
        // Intentional no-op.
    }

    @Override
    public void bulkRequestIconForPageUrl(ContentResolver cr, String where, IconListener listener) {
        WebViewChromium.recordWebViewApiCall(
                ApiCall.WEB_ICON_DATABASE_BULK_REQUEST_ICON_FOR_PAGE_URL);
        // Intentional no-op: hidden in base class.
    }

    @Override
    public void retainIconForPageUrl(String url) {
        WebViewChromium.recordWebViewApiCall(ApiCall.WEB_ICON_DATABASE_RETAIN_ICON_FOR_PAGE_URL);
        // Intentional no-op.
    }

    @Override
    public void releaseIconForPageUrl(String url) {
        WebViewChromium.recordWebViewApiCall(ApiCall.WEB_ICON_DATABASE_RELEASE_ICON_FOR_PAGE_URL);
        // Intentional no-op.
    }
}
