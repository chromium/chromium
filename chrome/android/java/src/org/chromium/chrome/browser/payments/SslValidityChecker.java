// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.WebContents;

/** SSL validity checker. */
@JNINamespace("payments")
public class SslValidityChecker {
    /**
     *  Returns a developer-facing error message for invalid SSL certificate state or an empty
     *  string when the SSL certificate is valid. Only EV_SECURE, SECURE, and
     *  SECURE_WITH_POLICY_INSTALLED_CERT are considered valid for web payments, unless
     *  --ignore-certificate-errors is specified on the command line.
     *
     * @param webContents The web contents whose SSL certificate state will be used for the error
     *                     message. Should not be null. A null |web_contents| parameter will return
     *                     an "Invalid certificate" error message.
     * @return A developer-facing error message about the SSL certificate state in the given web
     *         contents or an empty string when the SSL certificate is valid.
     */
    public static String getInvalidSslCertificateErrorMessage(WebContents webContents) {
        return SslValidityCheckerJni.get().getInvalidSslCertificateErrorMessage(webContents);
    }

    /**
     * Returns true for web contents that is allowed in a payment handler window.
     *
     * @param webContents The web contents to check.
     * @return Whether the web contents is a allowed in a payment handler window.
     */
    public static boolean isValidPageInPaymentHandlerWindow(WebContents webContents) {
        return SslValidityCheckerJni.get().isValidPageInPaymentHandlerWindow(webContents);
    }

    private SslValidityChecker() {}

    @NativeMethods
    interface Natives {
        String getInvalidSslCertificateErrorMessage(WebContents webContents);
        boolean isValidPageInPaymentHandlerWindow(WebContents webContents);
    }
}