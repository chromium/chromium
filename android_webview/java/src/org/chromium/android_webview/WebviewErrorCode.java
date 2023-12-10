// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.webkit.WebViewClient;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@Retention(RetentionPolicy.SOURCE)
@IntDef({
    WebviewErrorCode.ERROR_OK,
    WebviewErrorCode.ERROR_UNKNOWN,
    WebviewErrorCode.ERROR_HOST_LOOKUP,
    WebviewErrorCode.ERROR_UNSUPPORTED_AUTH_SCHEME,
    WebviewErrorCode.ERROR_AUTHENTICATION,
    WebviewErrorCode.ERROR_PROXY_AUTHENTICATION,
    WebviewErrorCode.ERROR_CONNECT,
    WebviewErrorCode.ERROR_IO,
    WebviewErrorCode.ERROR_TIMEOUT,
    WebviewErrorCode.ERROR_REDIRECT_LOOP,
    WebviewErrorCode.ERROR_UNSUPPORTED_SCHEME,
    WebviewErrorCode.ERROR_FAILED_SSL_HANDSHAKE,
    WebviewErrorCode.ERROR_BAD_URL,
    WebviewErrorCode.ERROR_FILE,
    WebviewErrorCode.ERROR_FILE_NOT_FOUND,
    WebviewErrorCode.ERROR_TOO_MANY_REQUESTS,
    WebviewErrorCode.ERROR_UNSAFE_RESOURCE
})
public @interface WebviewErrorCode {
    // Success
    int ERROR_OK = 0;
    // Generic error
    int ERROR_UNKNOWN = WebViewClient.ERROR_UNKNOWN;
    // Server or proxy hostname lookup failed
    int ERROR_HOST_LOOKUP = WebViewClient.ERROR_HOST_LOOKUP;
    // Unsupported authentication scheme (not basic or digest)
    int ERROR_UNSUPPORTED_AUTH_SCHEME = WebViewClient.ERROR_UNSUPPORTED_AUTH_SCHEME;
    // User authentication failed on server
    int ERROR_AUTHENTICATION = WebViewClient.ERROR_AUTHENTICATION;
    // User authentication failed on proxy
    int ERROR_PROXY_AUTHENTICATION = WebViewClient.ERROR_PROXY_AUTHENTICATION;
    // Failed to connect to the server
    int ERROR_CONNECT = WebViewClient.ERROR_CONNECT;
    // Failed to read or write to the server
    int ERROR_IO = WebViewClient.ERROR_IO;
    // Connection timed out
    int ERROR_TIMEOUT = WebViewClient.ERROR_TIMEOUT;
    // Too many redirects
    int ERROR_REDIRECT_LOOP = WebViewClient.ERROR_REDIRECT_LOOP;
    // Unsupported URI scheme
    int ERROR_UNSUPPORTED_SCHEME = WebViewClient.ERROR_UNSUPPORTED_SCHEME;
    // Failed to perform SSL handshake
    int ERROR_FAILED_SSL_HANDSHAKE = WebViewClient.ERROR_FAILED_SSL_HANDSHAKE;
    // Malformed URL
    int ERROR_BAD_URL = WebViewClient.ERROR_BAD_URL;
    // Generic file error
    int ERROR_FILE = WebViewClient.ERROR_FILE;
    // File not found
    int ERROR_FILE_NOT_FOUND = WebViewClient.ERROR_FILE_NOT_FOUND;
    // Too many requests during this load
    int ERROR_TOO_MANY_REQUESTS = WebViewClient.ERROR_TOO_MANY_REQUESTS;
    // Request was identified as a bad url by safebrowsing.
    int ERROR_UNSAFE_RESOURCE = WebViewClient.ERROR_UNSAFE_RESOURCE;
}
