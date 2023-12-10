// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.net.http.SslCertificate;
import android.net.http.SslError;
import android.util.Log;

import org.chromium.net.NetError;
import org.chromium.net.X509Util;

import java.security.KeyStoreException;
import java.security.NoSuchAlgorithmException;
import java.security.cert.CertificateException;
import java.security.cert.X509Certificate;

class SslUtil {
    private static final String TAG = "SslUtil";

    /** Creates an SslError object from a chromium net error code. */
    public static SslError sslErrorFromNetErrorCode(
            @NetError int error, SslCertificate cert, String url) {
        assert (error >= NetError.ERR_CERT_END && error <= NetError.ERR_CERT_COMMON_NAME_INVALID);
        switch (error) {
            case NetError.ERR_CERT_COMMON_NAME_INVALID:
                return new SslError(SslError.SSL_IDMISMATCH, cert, url);
            case NetError.ERR_CERT_DATE_INVALID:
                return new SslError(SslError.SSL_DATE_INVALID, cert, url);
            case NetError.ERR_CERT_KNOWN_INTERCEPTION_BLOCKED:
            case NetError.ERR_CERT_AUTHORITY_INVALID:
                return new SslError(SslError.SSL_UNTRUSTED, cert, url);
            default:
                break;
        }
        // Map all other codes to SSL_INVALID.
        return new SslError(SslError.SSL_INVALID, cert, url);
    }

    public static SslCertificate getCertificateFromDerBytes(byte[] derBytes) {
        if (derBytes == null) {
            return null;
        }

        try {
            X509Certificate x509Certificate = X509Util.createCertificateFromBytes(derBytes);
            return new SslCertificate(x509Certificate);
        } catch (CertificateException e) {
            // A SSL related exception must have occurred.  This shouldn't happen.
            Log.w(TAG, "Could not read certificate: " + e);
        } catch (KeyStoreException e) {
            // A SSL related exception must have occurred.  This shouldn't happen.
            Log.w(TAG, "Could not read certificate: " + e);
        } catch (NoSuchAlgorithmException e) {
            // A SSL related exception must have occurred.  This shouldn't happen.
            Log.w(TAG, "Could not read certificate: " + e);
        }
        return null;
    }
}
