// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.crypto;

import android.os.Bundle;
import android.os.PersistableBundle;

import androidx.annotation.AnyThread;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.security.GeneralSecurityException;
import java.security.Key;
import java.security.SecureRandom;
import java.util.Arrays;

import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.spec.IvParameterSpec;
import javax.crypto.spec.SecretKeySpec;

/**
 * Generates {@link Cipher} instances for encrypting session data that is temporarily stored.
 *
 * <p>When an Activity is sent to the background, Android gives it the opportunity to save state to
 * restore a user's session when the Activity is restarted. In addition to saving state to disk,
 * Android has a mechanism for saving instance state through {@link Bundle}s, which help
 * differentiate between users pausing and ending a session:
 *
 * <ul>
 *   <li>If the Activity is killed in the background (e.g. to free up resources for other
 *       Activities), Android gives a {@link Bundle} to the Activity when the user restarts the
 *       Activity. The {@link Bundle} is expected to be small and fast to generate, and is managed
 *       by Android.
 *   <li>If the Activity was explicitly killed (e.g. the user swiped away the task from Recent
 *       Tasks), Android does not restore the {@link Bundle} when the user restarts the Activity.
 * </ul>
 *
 * <p>To securely save temporary session data to disk: - Encrypt data with a {@link Cipher} from
 * {@link CipherFactory#getCipher(int)} before storing it. - Store {@link Cipher} parameters in the
 * Bundle via {@link CipherFactory#saveToBundle(Bundle)}.
 *
 * <p>Explicitly ending the session destroys the {@link Bundle}, making the previous session's data
 * unreadable.
 *
 * <p>WARNING: This class should not be used for any other purpose. It is not a general-purpose
 * encryption class. Moreover, the encryption scheme it implements is not cryptographically sound
 * and needs to be migrated to another one. See https://crbug.com/1440828.
 *
 * <p>NOTE: The new encryption key for tab state storage is cryptographically sound. However, it is
 * only used in C++ for the new tab state storage system and its presence here is entirely to
 * continue to leverage the existing bundle based persistence mechanism described above.
 */
@AnyThread
@NullMarked
public class CipherFactory {
    private static final String TAG = "CipherFactory";
    static final int NUM_BYTES = 16;

    static final String BUNDLE_IV = "org.chromium.content.browser.crypto.CipherFactory.IV";
    static final String PERSISTENT_BUNDLE_IV =
            "org.chromium.content.browser.crypto.CipherFactory.Persistent.IV";
    static final String BUNDLE_KEY = "org.chromium.content.browser.crypto.CipherFactory.KEY";
    static final String PERSISTENT_BUNDLE_KEY =
            "org.chromium.content.browser.crypto.CipherFactory.Persistent.KEY";
    static final String BUNDLE_TAB_STATE_STORAGE_KEY =
            "org.chromium.content.browser.crypto.CipherFactory.TAB_STATE_STORAGE_KEY";
    static final String PERSISTENT_BUNDLE_TAB_STATE_STORAGE_KEY =
            "org.chromium.content.browser.crypto.CipherFactory.Persistent.TAB_STATE_STORAGE_KEY";

    /** Holds intermediate data for the computation. */
    private static class CipherData {
        public final Key key;
        public final byte[] iv;

        public CipherData(Key key, byte[] iv) {
            this.key = key;
            this.iv = iv;
        }
    }

    /** Protects mData across threads. */
    private final Object mDataLock = new Object();

    /** Holds data for cipher generation. This is the key for the legacy TabPersistentStore. */
    private @Nullable CipherData mData;

    /**
     * Holds the encryption key for tab state storage. This is the key for the new TabStateStorage.
     */
    private byte @Nullable [] mTabStateStorageKey;

    /**
     * Constructor for a new {@link CipherFactory}. Each CipherFactory will use different encryption
     * keys.
     */
    public CipherFactory() {}

    /** Sets the encryption key for tab state storage, this key comes from C++. */
    public void setKeyForTabStateStorage(byte[] key) {
        ThreadUtils.assertOnUiThread();
        assert mTabStateStorageKey == null;
        mTabStateStorageKey = key;
    }

    /** Returns the encryption key for tab state storage. */
    public byte @Nullable [] getKeyForTabStateStorage() {
        ThreadUtils.assertOnUiThread();
        return mTabStateStorageKey;
    }

    /**
     * Creates a secure Cipher for encrypting data.
     *
     * @param opmode One of Cipher.{ENCRYPT,DECRYPT}_MODE.
     * @return A Cipher, or null if it is not possible to instantiate one.
     */
    public @Nullable Cipher getCipher(int opmode) {
        CipherData data = getCipherData(true);

        if (data != null) {
            try {
                Cipher cipher = Cipher.getInstance("AES/CBC/PKCS5Padding");
                cipher.init(opmode, data.key, new IvParameterSpec(data.iv));
                return cipher;
            } catch (GeneralSecurityException e) {
                // Can't do anything here.
            }
        }

        Log.e(TAG, "Error in creating cipher instance.");
        return null;
    }

    /**
     * Returns data required for generating the Cipher.
     * @param generateIfNeeded Generates data if needed.
     * @return Data to use for the Cipher, null if it couldn't be generated.
     */
    @Nullable CipherData getCipherData(boolean generateIfNeeded) {
        synchronized (mDataLock) {
            if (mData == null && generateIfNeeded) {
                // Poll random data to generate initialization parameters for the Cipher.
                try {
                    SecureRandom random = new SecureRandom();

                    byte[] iv = new byte[NUM_BYTES];
                    random.nextBytes(iv);

                    KeyGenerator generator = KeyGenerator.getInstance("AES");
                    generator.init(128, random);
                    mData = new CipherData(generator.generateKey(), iv);
                } catch (GeneralSecurityException e) {
                    Log.e(TAG, "Couldn't get generator instances.");
                    return null;
                }
            }
            return mData;
        }
    }

    /**
     * Saves the encryption data in a bundle. Expected to be called when an Activity saves its state
     * before being sent to the background.
     *
     * The IV *could* go into the first block of the payload. However, since the staleness of the
     * data is determined by whether or not it's able to be decrypted, the IV should not be read
     * from it.
     *
     * @param outState The data bundle to store data into.
     */
    public void saveToBundle(Bundle outState) {
        saveLegacyKey(outState);
        saveTabStateStorageKey(outState);
    }

    /**
     * Restores the encryption key from the given Bundle. Expected to be called when an Activity is
     * being restored after being killed in the background. If the Activity was explicitly killed by
     * the user, Android gives no Bundle (and therefore no key).
     *
     * @param savedInstanceState Bundle containing the Activity's previous state. Null if the user
     *     explicitly killed the Activity.
     * @return True if the data was restored successfully from the Bundle, or if the CipherData in
     *     use matches the Bundle contents.
     */
    public boolean restoreFromBundle(@Nullable Bundle savedInstanceState) {
        if (savedInstanceState == null) return false;

        boolean success = restoreLegacyKey(savedInstanceState);
        // This may not be present in all bundles.
        restoreTabStateStorageKey(savedInstanceState);

        return success;
    }

    private void saveLegacyKey(Bundle outState) {
        CipherData data = getCipherData(false);
        if (data == null) return;

        byte[] wrappedKey = data.key.getEncoded();
        if (wrappedKey != null && data.iv != null) {
            outState.putByteArray(BUNDLE_KEY, wrappedKey);
            outState.putByteArray(BUNDLE_IV, data.iv);
        }
    }

    private boolean restoreLegacyKey(Bundle savedInstanceState) {
        byte[] wrappedKey = savedInstanceState.getByteArray(BUNDLE_KEY);
        byte[] iv = savedInstanceState.getByteArray(BUNDLE_IV);
        if (wrappedKey == null || iv == null) return false;

        return updateCipherData(wrappedKey, iv);
    }

    /**
     * Saves the encryption data in a PersistableBundle. Expected to be called when an Activity
     * saves its state before being sent to the background, particularly during a device reboot or
     * app update.
     *
     * <p>The IV *could* go into the first block of the payload. However, since the staleness of the
     * data is determined by whether or not it's able to be decrypted, the IV should not be read
     * from it.
     *
     * @param outPersistentState The data bundle to store data into.
     */
    public void saveToPersistableBundle(PersistableBundle outPersistentState) {
        CipherData data = getCipherData(false);
        if (data == null) return;

        byte[] wrappedKey = data.key.getEncoded();
        int[] intKey = convertByteToIntArray(wrappedKey);
        int[] intIv = convertByteToIntArray(data.iv);

        if (wrappedKey != null && data.iv != null) {
            outPersistentState.putIntArray(PERSISTENT_BUNDLE_KEY, intKey);
            outPersistentState.putIntArray(PERSISTENT_BUNDLE_IV, intIv);
        }
    }

    /**
     * Restores the encryption key from the given PersistableBundle. Expected to be called when an
     * Activity is being restored after a device reboot or app update.
     *
     * @param persistentState PersistableBundle containing the Activity's previous state.
     * @return True if the data was restored successfully from the Bundle, or if the CipherData in
     *     use matches the Bundle contents.
     */
    public boolean restoreFromPersistableBundle(PersistableBundle persistentState) {
        if (persistentState == null) return false;

        int[] intKey = persistentState.getIntArray(PERSISTENT_BUNDLE_KEY);
        int[] intIv = persistentState.getIntArray(PERSISTENT_BUNDLE_IV);

        if (intKey == null || intIv == null) return false;

        byte[] wrappedKey = convertIntToByteArray(intKey);
        byte[] iv = convertIntToByteArray(intIv);

        return updateCipherData(wrappedKey, iv);
    }

    /** Clears state related to incognito from the PersistableBundle. */
    public void clearPersistentIncognitoState(PersistableBundle persistentState) {
        persistentState.remove(PERSISTENT_BUNDLE_KEY);
        persistentState.remove(PERSISTENT_BUNDLE_IV);
    }

    private boolean updateCipherData(byte[] wrappedKey, byte[] iv) {
        try {
            Key bundledKey = new SecretKeySpec(wrappedKey, "AES");
            synchronized (mDataLock) {
                if (mData == null) {
                    mData = new CipherData(bundledKey, iv);
                    return true;
                } else if (mData.key.equals(bundledKey) && Arrays.equals(mData.iv, iv)) {
                    return true;
                } else {
                    Log.e(TAG, "Attempted to restore different cipher data.");
                }
            }
        } catch (IllegalArgumentException e) {
            Log.e(TAG, "Error in restoring the key from the bundle.");
        }
        return false;
    }

    private void saveTabStateStorageKey(Bundle outState) {
        ThreadUtils.assertOnUiThread();
        if (mTabStateStorageKey != null) {
            outState.putByteArray(BUNDLE_TAB_STATE_STORAGE_KEY, mTabStateStorageKey);
        }
    }

    private void restoreTabStateStorageKey(Bundle savedInstanceState) {
        ThreadUtils.assertOnUiThread();
        byte[] key = savedInstanceState.getByteArray(BUNDLE_TAB_STATE_STORAGE_KEY);
        if (key != null) {
            if (mTabStateStorageKey == null) {
                mTabStateStorageKey = key;
            } else if (!constantTimeCompare(mTabStateStorageKey, key)) {
                Log.e(TAG, "Attempted to restore different tab state storage key.");
            }
        }
    }

    private static boolean constantTimeCompare(byte[] a, byte[] b) {
        if (a.length != b.length) return false;

        int ret = 0;
        for (int i = 0; i < a.length; i++) {
            ret |= (a[i] ^ b[i]);
        }
        // `ret` is non-zero iff some `a[i] ^ b[i]` differed.
        return ret == 0;
    }

    @VisibleForTesting
    static int[] convertByteToIntArray(byte[] bytes) {
        int[] intArray = new int[bytes.length];
        for (int i = 0; i < bytes.length; i++) {
            intArray[i] = (int) bytes[i];
        }
        return intArray;
    }

    @VisibleForTesting
    static byte[] convertIntToByteArray(int[] integers) {
        byte[] bytes = new byte[integers.length];
        for (int i = 0; i < integers.length; i++) {
            bytes[i] = (byte) integers[i];
            assert (int) bytes[i] == integers[i]
                    : "All integers should be in the range of possible byte values.";
        }
        return bytes;
    }
}
