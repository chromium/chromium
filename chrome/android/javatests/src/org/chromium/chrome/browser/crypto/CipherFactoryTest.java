// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.crypto;

import android.os.Bundle;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;

import java.security.SecureRandom;
import java.util.Arrays;

import javax.crypto.Cipher;

/**
 * Functional tests for the {@link CipherFactory}. Confirms that saving and restoring data works, as
 * well as that {@link Cipher} instances properly encrypt and decrypt data.
 *
 * <p>Tests that confirm that the class is thread-safe would require putting potentially flaky hooks
 * throughout the class to simulate artificial blockages.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class CipherFactoryTest {
    private static final byte[] INPUT_DATA = {1, 16, 84};

    private static byte[] getRandomBytes(int n) {
        byte[] ret = new byte[n];
        new SecureRandom().nextBytes(ret);
        return ret;
    }

    private CipherFactory mCipherFactory;

    @Before
    public void setUp() {
        mCipherFactory = new CipherFactory();
    }

    /**
     * {@link Cipher} instances initialized using the same parameters work in exactly the same way.
     */
    @Test
    @MediumTest
    public void testCipherUse() throws Exception {
        // Check encryption.
        Cipher aEncrypt = mCipherFactory.getCipher(Cipher.ENCRYPT_MODE);
        Cipher bEncrypt = mCipherFactory.getCipher(Cipher.ENCRYPT_MODE);
        byte[] output = sameOutputDifferentCiphers(INPUT_DATA, aEncrypt, bEncrypt);

        // Check decryption.
        Cipher aDecrypt = mCipherFactory.getCipher(Cipher.DECRYPT_MODE);
        Cipher bDecrypt = mCipherFactory.getCipher(Cipher.DECRYPT_MODE);
        byte[] decrypted = sameOutputDifferentCiphers(output, aDecrypt, bDecrypt);
        Assert.assertTrue(Arrays.equals(decrypted, INPUT_DATA));
    }

    /**
     * Restoring a {@link Bundle} containing the same parameters already in use by the {@link
     * CipherFactory} should keep the same keys.
     */
    @Test
    @MediumTest
    public void testSameBundleRestoration() throws Exception {
        // Create two bundles with the same saved state.
        Bundle aBundle = new Bundle();
        Bundle bBundle = new Bundle();

        byte[] sameIv = getRandomBytes(CipherFactory.NUM_BYTES);
        aBundle.putByteArray(CipherFactory.BUNDLE_IV, sameIv);
        bBundle.putByteArray(CipherFactory.BUNDLE_IV, sameIv);

        byte[] sameKey = getRandomBytes(CipherFactory.NUM_BYTES);
        aBundle.putByteArray(CipherFactory.BUNDLE_KEY, sameKey);
        bBundle.putByteArray(CipherFactory.BUNDLE_KEY, sameKey);

        // Restore using the first bundle, then the second. Both should succeed.
        Assert.assertTrue(mCipherFactory.restoreFromBundle(aBundle));
        Cipher aCipher = mCipherFactory.getCipher(Cipher.ENCRYPT_MODE);
        Assert.assertTrue(mCipherFactory.restoreFromBundle(bBundle));
        Cipher bCipher = mCipherFactory.getCipher(Cipher.ENCRYPT_MODE);

        // Make sure the CipherFactory instances are using the same key.
        sameOutputDifferentCiphers(INPUT_DATA, aCipher, bCipher);
    }

    /**
     * Restoring a {@link Bundle} containing a different set of parameters from those already in use
     * by the {@link CipherFactory} should fail. Any Ciphers created after the failed restoration
     * attempt should use the already-existing keys.
     */
    @Test
    @MediumTest
    public void testDifferentBundleRestoration() throws Exception {
        // Restore one set of parameters.
        Bundle aBundle = new Bundle();
        byte[] aIv = getRandomBytes(CipherFactory.NUM_BYTES);
        byte[] aKey = getRandomBytes(CipherFactory.NUM_BYTES);
        aBundle.putByteArray(CipherFactory.BUNDLE_IV, aIv);
        aBundle.putByteArray(CipherFactory.BUNDLE_KEY, aKey);
        Assert.assertTrue(mCipherFactory.restoreFromBundle(aBundle));
        Cipher aCipher = mCipherFactory.getCipher(Cipher.ENCRYPT_MODE);

        // Restore using a different set of parameters.
        Bundle bBundle = new Bundle();
        byte[] bIv = getRandomBytes(CipherFactory.NUM_BYTES);
        byte[] bKey = getRandomBytes(CipherFactory.NUM_BYTES);
        bBundle.putByteArray(CipherFactory.BUNDLE_IV, bIv);
        bBundle.putByteArray(CipherFactory.BUNDLE_KEY, bKey);
        Assert.assertFalse(mCipherFactory.restoreFromBundle(bBundle));
        Cipher bCipher = mCipherFactory.getCipher(Cipher.ENCRYPT_MODE);

        // Make sure they're using the same (original) key by encrypting the same data.
        sameOutputDifferentCiphers(INPUT_DATA, aCipher, bCipher);
    }

    /** Restoration from a {@link Bundle} missing data should fail. */
    @Test
    @MediumTest
    public void testIncompleteBundleRestoration() {
        // Make sure we handle the null case.
        Assert.assertFalse(mCipherFactory.restoreFromBundle(null));

        // Try restoring without the key.
        Bundle aBundle = new Bundle();
        byte[] iv = getRandomBytes(CipherFactory.NUM_BYTES);
        aBundle.putByteArray(CipherFactory.BUNDLE_IV, iv);
        Assert.assertFalse(mCipherFactory.restoreFromBundle(aBundle));

        // Try restoring without the initialization vector.
        Bundle bBundle = new Bundle();
        byte[] key = getRandomBytes(CipherFactory.NUM_BYTES);
        bBundle.putByteArray(CipherFactory.BUNDLE_KEY, key);
        Assert.assertFalse(mCipherFactory.restoreFromBundle(bBundle));
    }

    /**
     * Parameters should only be saved when they're needed by the {@link CipherFactory}. Restoring
     * parameters from a {@link Bundle} before this point should result in {@link Cipher}s using the
     * restored parameters instead of any generated ones.
     */
    @Test
    @MediumTest
    public void testRestorationSucceedsBeforeCipherCreated() {
        byte[] iv = getRandomBytes(CipherFactory.NUM_BYTES);
        byte[] key = getRandomBytes(CipherFactory.NUM_BYTES);
        Bundle bundle = new Bundle();
        bundle.putByteArray(CipherFactory.BUNDLE_IV, iv);
        bundle.putByteArray(CipherFactory.BUNDLE_KEY, key);

        // The keys should be initialized only after restoration.
        Assert.assertNull(mCipherFactory.getCipherData(false));
        Assert.assertTrue(mCipherFactory.restoreFromBundle(bundle));
        Assert.assertNotNull(mCipherFactory.getCipherData(false));
    }

    /**
     * If the {@link CipherFactory} has already generated parameters, restorations of different data
     * should fail. All {@link Cipher}s should use the generated parameters.
     */
    @Test
    @MediumTest
    public void testRestorationDiscardsAfterOtherCipherAlreadyCreated() throws Exception {
        byte[] iv = getRandomBytes(CipherFactory.NUM_BYTES);
        byte[] key = getRandomBytes(CipherFactory.NUM_BYTES);
        Bundle bundle = new Bundle();
        bundle.putByteArray(CipherFactory.BUNDLE_IV, iv);
        bundle.putByteArray(CipherFactory.BUNDLE_KEY, key);

        // The keys should be initialized after creating the cipher, so the keys shouldn't match.
        Cipher aCipher = mCipherFactory.getCipher(Cipher.ENCRYPT_MODE);
        Assert.assertFalse(mCipherFactory.restoreFromBundle(bundle));
        Cipher bCipher = mCipherFactory.getCipher(Cipher.ENCRYPT_MODE);

        // B's cipher should use the keys generated for A.
        sameOutputDifferentCiphers(INPUT_DATA, aCipher, bCipher);
    }

    /**
     * Data saved out to the {@link Bundle} should match what is held by the {@link CipherFactory}.
     */
    @Test
    @MediumTest
    public void testSavingToBundle() {
        // Nothing should get saved out before Cipher data exists.
        Bundle initialBundle = new Bundle();
        mCipherFactory.saveToBundle(initialBundle);
        Assert.assertFalse(initialBundle.containsKey(CipherFactory.BUNDLE_IV));
        Assert.assertFalse(initialBundle.containsKey(CipherFactory.BUNDLE_KEY));

        // Check that Cipher data gets saved if it exists.
        mCipherFactory.getCipher(Cipher.ENCRYPT_MODE);
        Bundle afterBundle = new Bundle();
        mCipherFactory.saveToBundle(afterBundle);
        Assert.assertTrue(afterBundle.containsKey(CipherFactory.BUNDLE_IV));
        Assert.assertTrue(afterBundle.containsKey(CipherFactory.BUNDLE_KEY));

        // Confirm the saved keys match by restoring it.
        Assert.assertTrue(mCipherFactory.restoreFromBundle(afterBundle));
    }

    /**
     * Confirm that the two {@link Cipher}s are functionally equivalent.
     *
     * @return The input after it has been operated on (e.g. decrypted or encrypted).
     */
    private byte[] sameOutputDifferentCiphers(byte[] input, Cipher aCipher, Cipher bCipher)
            throws Exception {
        Assert.assertNotNull(aCipher);
        Assert.assertNotNull(bCipher);
        Assert.assertNotSame(aCipher, bCipher);

        byte[] aOutput = aCipher.doFinal(input);
        byte[] bOutput = bCipher.doFinal(input);

        Assert.assertNotNull(aOutput);
        Assert.assertNotNull(bOutput);
        Assert.assertTrue(Arrays.equals(aOutput, bOutput));

        return aOutput;
    }
}
