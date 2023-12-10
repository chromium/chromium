// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.net.NetError;

/** This is a helper class to map native error code about loading a page to Android specific ones. */
public final class ErrorCodeConversionHelper {
    static @WebviewErrorCode int convertErrorCode(@NetError int netError) {
        // Note: many NetError.Error constants don't have an obvious mapping.
        // These will be handled by the default case, ERROR_UNKNOWN.
        switch (netError) {
            case NetError.ERR_UNSUPPORTED_AUTH_SCHEME:
                return WebviewErrorCode.ERROR_UNSUPPORTED_AUTH_SCHEME;

            case NetError.ERR_INVALID_AUTH_CREDENTIALS:
            case NetError.ERR_MISSING_AUTH_CREDENTIALS:
            case NetError.ERR_MISCONFIGURED_AUTH_ENVIRONMENT:
                return WebviewErrorCode.ERROR_AUTHENTICATION;

            case NetError.ERR_TOO_MANY_REDIRECTS:
                return WebviewErrorCode.ERROR_REDIRECT_LOOP;

            case NetError.ERR_UPLOAD_FILE_CHANGED:
                return WebviewErrorCode.ERROR_FILE_NOT_FOUND;

            case NetError.ERR_INVALID_URL:
                return WebviewErrorCode.ERROR_BAD_URL;

            case NetError.ERR_DISALLOWED_URL_SCHEME:
            case NetError.ERR_UNKNOWN_URL_SCHEME:
                return WebviewErrorCode.ERROR_UNSUPPORTED_SCHEME;

            case NetError.ERR_IO_PENDING:
            case NetError.ERR_NETWORK_IO_SUSPENDED:
                return WebviewErrorCode.ERROR_IO;

            case NetError.ERR_CONNECTION_TIMED_OUT:
            case NetError.ERR_TIMED_OUT:
                return WebviewErrorCode.ERROR_TIMEOUT;

            case NetError.ERR_FILE_TOO_BIG:
                return WebviewErrorCode.ERROR_FILE;

            case NetError.ERR_HOST_RESOLVER_QUEUE_TOO_LARGE:
            case NetError.ERR_INSUFFICIENT_RESOURCES:
            case NetError.ERR_OUT_OF_MEMORY:
                return WebviewErrorCode.ERROR_TOO_MANY_REQUESTS;

            case NetError.ERR_BLOCKED_BY_ADMINISTRATOR:
            case NetError.ERR_CONNECTION_CLOSED:
            case NetError.ERR_CONNECTION_RESET:
            case NetError.ERR_CONNECTION_REFUSED:
            case NetError.ERR_CONNECTION_ABORTED:
            case NetError.ERR_CONNECTION_FAILED:
            case NetError.ERR_SOCKET_NOT_CONNECTED:
                return WebviewErrorCode.ERROR_CONNECT;

            case NetError.ERR_INTERNET_DISCONNECTED:
            case NetError.ERR_ADDRESS_INVALID:
            case NetError.ERR_ADDRESS_UNREACHABLE:
            case NetError.ERR_NAME_NOT_RESOLVED:
            case NetError.ERR_NAME_RESOLUTION_FAILED:
            case NetError.ERR_ICANN_NAME_COLLISION:
                return WebviewErrorCode.ERROR_HOST_LOOKUP;

            case NetError.ERR_SSL_PROTOCOL_ERROR:
            case NetError.ERR_SSL_CLIENT_AUTH_CERT_NEEDED:
            case NetError.ERR_TUNNEL_CONNECTION_FAILED:
            case NetError.ERR_NO_SSL_VERSIONS_ENABLED:
            case NetError.ERR_SSL_VERSION_OR_CIPHER_MISMATCH:
            case NetError.ERR_SSL_RENEGOTIATION_REQUESTED:
            case NetError.ERR_BAD_SSL_CLIENT_AUTH_CERT:
            case NetError.ERR_SSL_NO_RENEGOTIATION:
            case NetError.ERR_SSL_DECOMPRESSION_FAILURE_ALERT:
            case NetError.ERR_SSL_BAD_RECORD_MAC_ALERT:
            case NetError.ERR_SSL_CLIENT_AUTH_PRIVATE_KEY_ACCESS_DENIED:
            case NetError.ERR_SSL_CLIENT_AUTH_CERT_NO_PRIVATE_KEY:
                return WebviewErrorCode.ERROR_FAILED_SSL_HANDSHAKE;

            case NetError.ERR_PROXY_AUTH_UNSUPPORTED:
            case NetError.ERR_PROXY_AUTH_REQUESTED:
            case NetError.ERR_PROXY_CONNECTION_FAILED:
            case NetError.ERR_UNEXPECTED_PROXY_AUTH:
                return WebviewErrorCode.ERROR_PROXY_AUTHENTICATION;

                // The certificate errors are handled by onReceivedSslError
                // and don't need to be reported here.
            case NetError.ERR_CERT_KNOWN_INTERCEPTION_BLOCKED:
            case NetError.ERR_CERT_COMMON_NAME_INVALID:
            case NetError.ERR_CERT_DATE_INVALID:
            case NetError.ERR_CERT_AUTHORITY_INVALID:
            case NetError.ERR_CERT_CONTAINS_ERRORS:
            case NetError.ERR_CERT_NO_REVOCATION_MECHANISM:
            case NetError.ERR_CERT_UNABLE_TO_CHECK_REVOCATION:
            case NetError.ERR_CERT_REVOKED:
            case NetError.ERR_CERT_INVALID:
            case NetError.ERR_CERT_WEAK_SIGNATURE_ALGORITHM:
            case NetError.ERR_CERT_NON_UNIQUE_NAME:
                return WebviewErrorCode.ERROR_OK;

            default:
                return WebviewErrorCode.ERROR_UNKNOWN;
        }
    }

    // Do not instantiate this class.
    private ErrorCodeConversionHelper() {}
}
