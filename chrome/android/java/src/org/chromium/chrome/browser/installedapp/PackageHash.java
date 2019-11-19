// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.installedapp;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.browser.crypto.ByteArrayGenerator;

import java.io.IOException;
import java.security.GeneralSecurityException;
import java.security.InvalidKeyException;
import java.security.Key;
import java.security.NoSuchAlgorithmException;

import javax.crypto.Mac;
import javax.crypto.spec.SecretKeySpec;

/**
 * Helper class for retrieving a device-unique hash for an Android package name.
 *
 * This is used to counter a potential timing attack against the getInstalledRelatedApps API, by
 * adding a pseudo-random time delay to the query. The delay is a hash of a globally unique
 * identifier for the current browser session, and the package name, which means websites are unable
 * to predict what each user's delay will be, nor compare between apps on a given device.
 *
 * The salt is generated per browser session (not per query, page load, user or device) because it
 * we want it to change "occasionally" -- not too frequently, but sometimes. Each time the salt
 * changes, it gives the site another opportunity to collect data that could be averaged out to
 * cancel out the random noise and find the true timing. So we don't want it changing too often.
 * However, it does need to change periodically: because installing or uninstalling the app creates
 * a noticeable change to the timing of the operation, we need to occasionally change the salt to
 * create plausible deniability (the attacker can't tell the difference between the salt changing
 * and the app being installed/uninstalled).
 */
class PackageHash {
    // Global salt string for the life of the browser process. A unique salt is generated for
    // each run of the browser process that will be stable for its lifetime.
    private static byte[] sSalt;

    /**
     * Returns a SHA-256 hash of the package name, truncated to a 16-bit integer.
     */
    public static short hashForPackage(String packageName) {
        byte[] salt = getGlobalSalt();
        Mac hasher;
        try {
            hasher = Mac.getInstance("HmacSHA256");
        } catch (NoSuchAlgorithmException e) {
            // Should never happen.
            throw new RuntimeException(e);
        }

        byte[] packageNameBytes = ApiCompatibilityUtils.getBytesUtf8(packageName);

        Key key = new SecretKeySpec(salt, "HmacSHA256");
        try {
            hasher.init(key);
        } catch (InvalidKeyException e) {
            // Should never happen.
            throw new RuntimeException(e);
        }
        byte[] digest = hasher.doFinal(packageNameBytes);
        // Take just the first two bytes of the digest.
        int hash = ((((int) digest[0]) & 0xff) << 8) | (((int) digest[1]) & 0xff);
        return (short) hash;
    }

    /**
     * Gets the global salt for the current browser session.
     *
     * If one does not exist, generates one using a PRNG and caches it, then returns it.
     */
    private static byte[] getGlobalSalt() {
        if (sSalt == null) {
            try {
                sSalt = new ByteArrayGenerator().getBytes(20);
            } catch (IOException | GeneralSecurityException e) {
                // If this happens, the crypto source is messed up and we want the browser to crash.
                throw new RuntimeException(e);
            }
        }

        return sSalt;
    }

    @VisibleForTesting
    public static void setGlobalSaltForTesting(byte[] salt) {
        sSalt = salt;
    }
}
