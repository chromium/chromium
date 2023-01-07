// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.Signature;

import java.io.ByteArrayInputStream;
import java.io.InputStream;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.security.cert.CertificateEncodingException;
import java.security.cert.CertificateException;
import java.security.cert.CertificateFactory;
import java.security.cert.X509Certificate;
import java.util.ArrayList;
import java.util.List;

/**
 * This class provides package checking related methods.
 */
public class PackageUtils {
    private static final String TAG = "PackageUtils";
    private static final char[] HEX_CHAR_LOOKUP = "0123456789ABCDEF".toCharArray();

    /**
     * Retrieves the version of the given package installed on the device.
     *
     * @param context Any context.
     * @param packageName Name of the package to find.
     * @return The package's version code if found, -1 otherwise.
     */
    public static int getPackageVersion(Context context, String packageName) {
        int versionCode = -1;
        PackageManager pm = context.getPackageManager();
        try {
            PackageInfo packageInfo = pm.getPackageInfo(packageName, 0);
            if (packageInfo != null) versionCode = packageInfo.versionCode;
        } catch (PackageManager.NameNotFoundException e) {
            // Do nothing, versionCode stays -1
        }
        return versionCode;
    }

    /**
     * Checks if the app has been installed on the system.
     * @return true if the PackageManager reports that the app is installed, false otherwise.
     * @param context Any context.
     * @param packageName Name of the package to check.
     */
    public static boolean isPackageInstalled(Context context, String packageName) {
        PackageManager pm = context.getPackageManager();
        try {
            pm.getPackageInfo(packageName, 0);
        } catch (PackageManager.NameNotFoundException e) {
            return false;
        }
        return true;
    }

    /**
     * Computes the SHA256 certificates for the given package name. The app with the given package
     * name has to be installed on device. The output will be a list of 30 long HEX strings with :
     * between each value. There will be one string for each signature the app is signed with.
     * @param packageName The package name to query the signature for.
     * @return The SHA256 certificate for the package name.
     */
    @SuppressLint("PackageManagerGetSignatures")
    // https://stackoverflow.com/questions/39192844/android-studio-warning-when-using-packagemanager-get-signatures
    public static List<String> getCertificateSHA256FingerprintForPackage(
            PackageManager pm, String packageName) {
        PackageInfo packageInfo;
        try {
            packageInfo = pm.getPackageInfo(packageName, PackageManager.GET_SIGNATURES);
        } catch (PackageManager.NameNotFoundException e) {
            return null;
        }

        if (packageInfo == null) return null;

        ArrayList<String> fingerprints = new ArrayList<>(packageInfo.signatures.length);

        for (Signature signature : packageInfo.signatures) {
            InputStream input = new ByteArrayInputStream(signature.toByteArray());
            String hexString = null;
            try {
                X509Certificate certificate =
                        (X509Certificate) CertificateFactory.getInstance("X509")
                                .generateCertificate(input);
                hexString = byteArrayToHexString(
                        MessageDigest.getInstance("SHA256").digest(certificate.getEncoded()));
            } catch (CertificateEncodingException e) {
                Log.w(TAG, "Certificate type X509 encoding failed");
                return null;
            } catch (CertificateException | NoSuchAlgorithmException e) {
                // This shouldn't happen.
            }

            fingerprints.add(hexString);
        }

        return fingerprints;
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

    private PackageUtils() {
        // Hide constructor
    }
}
