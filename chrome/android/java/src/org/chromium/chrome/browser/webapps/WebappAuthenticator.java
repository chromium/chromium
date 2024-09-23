// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Context;
import android.util.Log;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.security.GeneralSecurityException;
import java.security.NoSuchAlgorithmException;
import java.security.SecureRandom;

import javax.crypto.KeyGenerator;
import javax.crypto.Mac;
import javax.crypto.SecretKey;
import javax.crypto.spec.SecretKeySpec;

/**
 * Authenticate the source of Intents to launch web apps (see {@link #WebappLauncherActivity}).
 *
 * Chrome does not keep a store of valid URLs for installed web apps (because it cannot know when
 * any have been uninstalled). Therefore, upon installation, it tells the Launcher a message
 * authentication code (MAC) along with the URL for the web app, and then Chrome can verify the MAC
 * when starting e.g. {@link #WebappLauncherActivity}. Chrome can thus distinguish between
 * legitimate, installed web apps and arbitrary other URLs.
 */
public class WebappAuthenticator {
    private static final String TAG = "WebappAuthenticator";
    private static final String MAC_ALGORITHM_NAME = "HmacSHA256";
    private static final String MAC_KEY_BASENAME = "webapp-authenticator";
    private static final int MAC_KEY_BYTE_COUNT = 32;
    private static final Object sLock = new Object();

    private static SecretKey sKey;

    /**
     * @see #getMacForUrl
     * @param url The URL to validate.
     * @param mac The bytes of a previously-calculated MAC.
     * @return true if the MAC is a valid MAC for the URL, false otherwise.
     */
    public static boolean isUrlValid(String url, byte[] mac) {
        byte[] goodMac;
        // TODO(crbug.com/41198030): Temporarily allowing disk access until more permanent fix is
        // in.
        goodMac = getMacForUrl(url);
        if (goodMac == null) {
            return false;
        }
        return constantTimeAreArraysEqual(goodMac, mac);
    }

    /**
     * @see #isUrlValid
     *
     * @param url A URL for which to calculate a MAC.
     *
     * @return The bytes of a MAC for the URL, or null if a secure MAC was not available.
     */
    public static byte[] getMacForUrl(String url) {
        Mac mac = getMac();
        if (mac == null) {
            return null;
        }
        return mac.doFinal(ApiCompatibilityUtils.getBytesUtf8(url));
    }

    // TODO(palmer): Put this method, and as much of this class as possible, in a utility class.
    private static boolean constantTimeAreArraysEqual(byte[] a, byte[] b) {
        if (a.length != b.length) {
            return false;
        }

        int result = 0;
        for (int i = 0; i < a.length; i++) {
            result |= a[i] ^ b[i];
        }
        return result == 0;
    }

    private static SecretKey readKeyFromFile(
            Context context, String basename, String algorithmName) {
        FileInputStream input = null;
        File file = context.getFileStreamPath(basename);
        try {
            if (file.length() != MAC_KEY_BYTE_COUNT) {
                Log.w(TAG, "Could not read key from '" + file + "': invalid file contents");
                return null;
            }

            byte[] keyBytes = new byte[MAC_KEY_BYTE_COUNT];
            input = new FileInputStream(file);
            if (MAC_KEY_BYTE_COUNT != input.read(keyBytes)) {
                return null;
            }

            try {
                return new SecretKeySpec(keyBytes, algorithmName);
            } catch (IllegalArgumentException e) {
                return null;
            }
        } catch (Exception e) {
            Log.w(TAG, "Could not read key from '" + file + "': " + e);
            return null;
        } finally {
            try {
                if (input != null) {
                    input.close();
                }
            } catch (IOException e) {
                Log.e(TAG, "Could not close key input stream '" + file + "': " + e);
            }
        }
    }

    private static boolean writeKeyToFile(Context context, String basename, SecretKey key) {
        File file = context.getFileStreamPath(basename);
        byte[] keyBytes = key.getEncoded();
        FileOutputStream output = null;
        if (MAC_KEY_BYTE_COUNT != keyBytes.length) {
            Log.e(
                    TAG,
                    "writeKeyToFile got key encoded bytes length "
                            + keyBytes.length
                            + "; expected "
                            + MAC_KEY_BYTE_COUNT);
            return false;
        }

        try {
            output = new FileOutputStream(file);
            output.write(keyBytes);
            return true;
        } catch (Exception e) {
            Log.e(TAG, "Could not write key to '" + file + "': " + e);
            return false;
        } finally {
            try {
                if (output != null) {
                    output.close();
                }
            } catch (IOException e) {
                Log.e(TAG, "Could not close key output stream '" + file + "': " + e);
            }
        }
    }

    private static SecretKey getKey() {
        synchronized (sLock) {
            if (sKey == null) {
                Context context = ContextUtils.getApplicationContext();
                SecretKey key = readKeyFromFile(context, MAC_KEY_BASENAME, MAC_ALGORITHM_NAME);
                if (key != null) {
                    sKey = key;
                    return sKey;
                }

                sKey = generateMacKey();
                if (!writeKeyToFile(context, MAC_KEY_BASENAME, sKey)) {
                    sKey = null;
                    return null;
                }
                return sKey;
            }
            return sKey;
        }
    }

    /** Generates the authentication encryption key in a background thread (if necessary). */
    private static SecretKey generateMacKey() {
        if (sKey != null) {
            return sKey;
        }
        try {
            KeyGenerator generator = KeyGenerator.getInstance(MAC_ALGORITHM_NAME);
            SecureRandom random = new SecureRandom();
            generator.init(MAC_KEY_BYTE_COUNT * 8, random);
            return generator.generateKey();
        } catch (NoSuchAlgorithmException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * @return A Mac, or null if it is not possible to instantiate one.
     */
    private static Mac getMac() {
        try {
            SecretKey key = getKey();
            if (key == null) {
                // getKey should have invoked triggerMacKeyGeneration, which should have set the
                // random seed and generated a key from it. If not, there is a problem with the
                // random number generator, and we must not claim that authentication can work.
                return null;
            }
            Mac mac = Mac.getInstance(MAC_ALGORITHM_NAME);
            mac.init(key);
            return mac;
        } catch (GeneralSecurityException e) {
            Log.w(TAG, "Error in creating MAC instance", e);
            return null;
        }
    }
}
