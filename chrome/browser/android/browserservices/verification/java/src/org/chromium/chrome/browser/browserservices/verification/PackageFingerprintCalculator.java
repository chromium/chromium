// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.verification;

import android.annotation.SuppressLint;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;

import org.chromium.base.Log;

import java.io.ByteArrayInputStream;
import java.io.InputStream;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.security.cert.CertificateEncodingException;
import java.security.cert.CertificateException;
import java.security.cert.CertificateFactory;
import java.security.cert.X509Certificate;

/**
 * Generates the fingerprint for a given package on the device.
 *
 * See {@link #getCertificateSHA256FingerprintForPackage}.
 */
public class PackageFingerprintCalculator {
    private static final String TAG = "SignatureFormatter";
    private static final char[] HEX_CHAR_LOOKUP = "0123456789ABCDEF".toCharArray();

    private static PackageInfo getPackageInfo(PackageManager pm, String packageName) {
        PackageInfo packageInfo = null;
        try {
            packageInfo = pm.getPackageInfo(packageName, PackageManager.GET_SIGNATURES);
        } catch (PackageManager.NameNotFoundException e) {
            // Will return null if there is no package found.
        }
        return packageInfo;
    }

    /**
     * Computes the SHA256 certificate for the given package name. The app with the given package
     * name has to be installed on device. The output will be a 30 long HEX string with : between
     * each value.
     * @param packageName The package name to query the signature for.
     * @return The SHA256 certificate for the package name.
     */
    @SuppressLint("PackageManagerGetSignatures")
    // https://stackoverflow.com/questions/39192844/android-studio-warning-when-using-packagemanager-get-signatures
    public static String getCertificateSHA256FingerprintForPackage(
            PackageManager pm, String packageName) {
        PackageInfo packageInfo = getPackageInfo(pm, packageName);
        if (packageInfo == null) return null;

        InputStream input = new ByteArrayInputStream(packageInfo.signatures[0].toByteArray());
        String hexString = null;
        try {
            X509Certificate certificate =
                    (X509Certificate) CertificateFactory.getInstance("X509").generateCertificate(
                            input);
            hexString = byteArrayToHexString(
                    MessageDigest.getInstance("SHA256").digest(certificate.getEncoded()));
        } catch (CertificateEncodingException e) {
            Log.w(TAG, "Certificate type X509 encoding failed");
        } catch (CertificateException | NoSuchAlgorithmException e) {
            // This shouldn't happen.
        }
        return hexString;
    }

    /**
     * Converts a byte array to hex string with : inserted between each element.
     * @param byteArray The array to be converted.
     * @return A string with two letters representing each byte and : in between.
     */
    static String byteArrayToHexString(byte[] byteArray) {
        StringBuilder hexString = new StringBuilder(byteArray.length * 3 - 1);
        for (int i = 0; i < byteArray.length; ++i) {
            hexString.append(HEX_CHAR_LOOKUP[(byteArray[i] & 0xf0) >>> 4]);
            hexString.append(HEX_CHAR_LOOKUP[byteArray[i] & 0xf]);
            if (i < (byteArray.length - 1)) hexString.append(':');
        }
        return hexString.toString();
    }
}
