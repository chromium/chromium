// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.net.http.X509TrustManagerExtensions;
import android.view.contentcapture.ContentCaptureSession;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.security.cert.CertificateException;
import java.security.cert.X509Certificate;
import java.util.List;

/** Interface to call unreleased Android APIs that are guarded by aconfig flags. */
@NullMarked
public interface AconfigFlaggedApiDelegate {
    /** Call ContentCaptureSession.flush() if supported, otherwise no-op. */
    default void flushContentCaptureSession(ContentCaptureSession session) {}

    /**
     * Call the CertificateTransparency-enabled cert verification platform API if its supported,
     * otherwise call the normal non-CT cert verification API.
     */
    default List<X509Certificate> checkServerTrusted(
            X509TrustManagerExtensions tm,
            X509Certificate[] chain,
            String authType,
            String host,
            byte @Nullable [] ocspResponse,
            byte @Nullable [] sctList)
            throws CertificateException {
        return tm.checkServerTrusted(chain, authType, host);
    }
}
