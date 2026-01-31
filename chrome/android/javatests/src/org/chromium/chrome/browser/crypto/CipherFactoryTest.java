// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.crypto;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.os.Bundle;
import android.os.PersistableBundle;

import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

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
    @UiThreadTest
    public void testCipherUse() throws Exception {
        // Check encryption.
        Cipher aEncrypt = mCipherFactory.getCipher(Cipher.ENCRYPT_MODE);
        Cipher bEncrypt = mCipherFactory.getCipher(Cipher.ENCRYPT_MODE);
        byte[] output = sameOutputDifferentCiphers(INPUT_DATA, aEncrypt, bEncrypt);

        // Check decryption.
        Cipher aDecrypt = mCipherFactory.getCipher(Cipher.DECRYPT_MODE);
        Cipher bDecrypt = mCipherFactory.getCipher(Cipher.DECRYPT_MODE);
        byte[] decrypted = sameOutputDifferentCiphers(output, aDecrypt, bDecrypt);
        assertTrue(Arrays.equals(decrypted, INPUT_DATA));
    }

    /**
     * Restoring a {@link Bundle} containing the same parameters already in use by the {@link
     * CipherFactory} should keep the same keys.
     */
    @Test
    @MediumTest
    @UiThreadTest
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
        assertTrue(mCipherFactory.restoreFromBundle(aBundle));
        Cipher aCipher = mCipherFactory.getCipher(Cipher.ENCRYPT_MODE);
        assertTrue(mCipherFactory.restoreFromBundle(bBundle));
        Cipher bCipher = mCipherFactory.getCipher(Cipher.ENCRYPT_MODE);

        // Make sure the CipherFactory instances are using the same key.
        sameOutputDifferentCiphers(INPUT_DATA, aCipher, bCipher);
    }

    /**
     * Restoring a {@link PersistableBundle} containing the same parameters already in use by the
     * {@link CipherFactory} should keep the same keys.
     */
    @Test
    @MediumTest
    public void testSamePersistableBundleRestoration() throws Exception {
        // Create two bundles with the same saved state.
        PersistableBundle aPersistableBundle = new PersistableBundle();
        PersistableBundle bPersistableBundle = new PersistableBundle();

        byte[] sameIv = getRandomBytes(CipherFactory.NUM_BYTES);
        aPersistableBundle.putIntArray(
                CipherFactory.PERSISTENT_BUNDLE_IV, CipherFactory.convertByteToIntArray(sameIv));
        bPersistableBundle.putIntArray(
                CipherFactory.PERSISTENT_BUNDLE_IV, CipherFactory.convertByteToIntArray(sameIv));

        byte[] sameKey = getRandomBytes(CipherFactory.NUM_BYTES);
        aPersistableBundle.putIntArray(
                CipherFactory.PERSISTENT_BUNDLE_KEY, CipherFactory.convertByteToIntArray(sameKey));
        bPersistableBundle.putIntArray(
                CipherFactory.PERSISTENT_BUNDLE_KEY, CipherFactory.convertByteToIntArray(sameKey));

        // Restore using the first bundle, then the second. Both should succeed.
        assertTrue(mCipherFactory.restoreFromPersistableBundle(aPersistableBundle));
        Cipher aCipher = mCipherFactory.getCipher(Cipher.ENCRYPT_MODE);
        assertTrue(mCipherFactory.restoreFromPersistableBundle(bPersistableBundle));
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
    @UiThreadTest
    public void testDifferentBundleRestoration() throws Exception {
        // Restore one set of parameters.
        Bundle aBundle = new Bundle();
        byte[] aIv = getRandomBytes(CipherFactory.NUM_BYTES);
        byte[] aKey = getRandomBytes(CipherFactory.NUM_BYTES);
        aBundle.putByteArray(CipherFactory.BUNDLE_IV, aIv);
        aBundle.putByteArray(CipherFactory.BUNDLE_KEY, aKey);
        assertTrue(mCipherFactory.restoreFromBundle(aBundle));
        Cipher aCipher = mCipherFactory.getCipher(Cipher.ENCRYPT_MODE);

        // Restore using a different set of parameters.
        Bundle bBundle = new Bundle();
        byte[] bIv = getRandomBytes(CipherFactory.NUM_BYTES);
        byte[] bKey = getRandomBytes(CipherFactory.NUM_BYTES);
        bBundle.putByteArray(CipherFactory.BUNDLE_IV, bIv);
        bBundle.putByteArray(CipherFactory.BUNDLE_KEY, bKey);
        assertFalse(mCipherFactory.restoreFromBundle(bBundle));
        Cipher bCipher = mCipherFactory.getCipher(Cipher.ENCRYPT_MODE);

        // Make sure they're using the same (original) key by encrypting the same data.
        sameOutputDifferentCiphers(INPUT_DATA, aCipher, bCipher);
    }

    /**
     * Restoring a {@link PersistableBundle} containing a different set of parameters from those
     * already in use by the {@link CipherFactory} should fail. Any Ciphers created after the failed
     * restoration attempt should use the already-existing keys.
     */
    @Test
    @MediumTest
    public void testDifferentPersistableBundleRestoration() throws Exception {
        // Restore one set of parameters.
        PersistableBundle aPersistableBundle = createPersistableBundleWithRandomKey();
        assertTrue(mCipherFactory.restoreFromPersistableBundle(aPersistableBundle));
        Cipher aCipher = mCipherFactory.getCipher(Cipher.ENCRYPT_MODE);

        // Restore using a different set of parameters.
        PersistableBundle bPersistableBundle = createPersistableBundleWithRandomKey();
        assertFalse(mCipherFactory.restoreFromPersistableBundle(bPersistableBundle));
        Cipher bCipher = mCipherFactory.getCipher(Cipher.ENCRYPT_MODE);

        // Make sure they're using the same (original) key by encrypting the same data.
        sameOutputDifferentCiphers(INPUT_DATA, aCipher, bCipher);
    }

    /** Restoration from a {@link Bundle} missing data should fail. */
    @Test
    @MediumTest
    @UiThreadTest
    public void testIncompleteBundleRestoration() {
        // Make sure we handle the null case.
        assertFalse(mCipherFactory.restoreFromBundle(null));

        // Try restoring without the key.
        Bundle aBundle = new Bundle();
        byte[] iv = getRandomBytes(CipherFactory.NUM_BYTES);
        aBundle.putByteArray(CipherFactory.BUNDLE_IV, iv);
        assertFalse(mCipherFactory.restoreFromBundle(aBundle));

        // Try restoring without the initialization vector.
        Bundle bBundle = new Bundle();
        byte[] key = getRandomBytes(CipherFactory.NUM_BYTES);
        bBundle.putByteArray(CipherFactory.BUNDLE_KEY, key);
        assertFalse(mCipherFactory.restoreFromBundle(bBundle));
    }

    /** Restoration from a {@link PersistableBundle} missing data should fail. */
    @Test
    @MediumTest
    public void testIncompletePersistableBundleRestoration() {
        // Make sure we handle the null case.
        assertFalse(mCipherFactory.restoreFromPersistableBundle(null));

        // Try restoring without the key.
        PersistableBundle aPersistableBundle = new PersistableBundle();
        byte[] iv = getRandomBytes(CipherFactory.NUM_BYTES);
        aPersistableBundle.putIntArray(
                CipherFactory.PERSISTENT_BUNDLE_IV, CipherFactory.convertByteToIntArray(iv));
        assertFalse(mCipherFactory.restoreFromPersistableBundle(aPersistableBundle));

        // Try restoring without the initialization vector.
        PersistableBundle bPersistableBundle = new PersistableBundle();
        byte[] key = getRandomBytes(CipherFactory.NUM_BYTES);
        bPersistableBundle.putIntArray(
                CipherFactory.PERSISTENT_BUNDLE_KEY, CipherFactory.convertByteToIntArray(key));
        assertFalse(mCipherFactory.restoreFromPersistableBundle(bPersistableBundle));
    }

    /**
     * Parameters should only be saved when they're needed by the {@link CipherFactory}. Restoring
     * parameters from a {@link Bundle} before this point should result in {@link Cipher}s using the
     * restored parameters instead of any generated ones.
     */
    @Test
    @MediumTest
    @UiThreadTest
    public void testRestorationSucceedsBeforeCipherCreated() {
        byte[] iv = getRandomBytes(CipherFactory.NUM_BYTES);
        byte[] key = getRandomBytes(CipherFactory.NUM_BYTES);
        Bundle bundle = new Bundle();
        bundle.putByteArray(CipherFactory.BUNDLE_IV, iv);
        bundle.putByteArray(CipherFactory.BUNDLE_KEY, key);

        // The keys should be initialized only after restoration.
        assertNull(mCipherFactory.getCipherData(false));
        assertTrue(mCipherFactory.restoreFromBundle(bundle));
        assertNotNull(mCipherFactory.getCipherData(false));
    }

    /**
     * Parameters should only be saved when they're needed by the {@link CipherFactory}. Restoring
     * parameters from a {@link PersistableBundle} before this point should result in {@link
     * Cipher}s using the restored parameters instead of any generated ones.
     */
    @Test
    @MediumTest
    public void testRestorationSucceedsBeforeCipherCreated_PersistableBundle() {
        PersistableBundle persistableBundle = createPersistableBundleWithRandomKey();

        // The keys should be initialized only after restoration.
        assertNull(mCipherFactory.getCipherData(false));
        assertTrue(mCipherFactory.restoreFromPersistableBundle(persistableBundle));
        assertNotNull(mCipherFactory.getCipherData(false));
    }

    /**
     * If the {@link CipherFactory} has already generated parameters, restorations of different data
     * should fail. All {@link Cipher}s should use the generated parameters.
     */
    @Test
    @MediumTest
    @UiThreadTest
    public void testRestorationDiscardsAfterOtherCipherAlreadyCreated() throws Exception {
        byte[] iv = getRandomBytes(CipherFactory.NUM_BYTES);
        byte[] key = getRandomBytes(CipherFactory.NUM_BYTES);
        Bundle bundle = new Bundle();
        bundle.putByteArray(CipherFactory.BUNDLE_IV, iv);
        bundle.putByteArray(CipherFactory.BUNDLE_KEY, key);

        // The keys should be initialized after creating the cipher, so the keys shouldn't match.
        Cipher aCipher = mCipherFactory.getCipher(Cipher.ENCRYPT_MODE);
        assertFalse(mCipherFactory.restoreFromBundle(bundle));
        Cipher bCipher = mCipherFactory.getCipher(Cipher.ENCRYPT_MODE);

        // B's cipher should use the keys generated for A.
        sameOutputDifferentCiphers(INPUT_DATA, aCipher, bCipher);
    }

    /**
     * If the {@link CipherFactory} has already generated parameters, restorations of different data
     * should fail. All {@link Cipher}s should use the generated parameters.
     */
    @Test
    @MediumTest
    public void testRestorationDiscardsAfterOtherCipherAlreadyCreated_PersistableBundle()
            throws Exception {
        PersistableBundle persistableBundle = createPersistableBundleWithRandomKey();

        // The keys should be initialized after creating the cipher, so the keys shouldn't match.
        Cipher aCipher = mCipherFactory.getCipher(Cipher.ENCRYPT_MODE);
        assertFalse(mCipherFactory.restoreFromPersistableBundle(persistableBundle));
        Cipher bCipher = mCipherFactory.getCipher(Cipher.ENCRYPT_MODE);

        // B's cipher should use the keys generated for A.
        sameOutputDifferentCiphers(INPUT_DATA, aCipher, bCipher);
    }

    /**
     * Data saved out to the {@link Bundle} should match what is held by the {@link CipherFactory}.
     */
    @Test
    @MediumTest
    @UiThreadTest
    public void testSavingToBundle() {
        // Nothing should get saved out before Cipher data exists.
        Bundle initialBundle = new Bundle();
        mCipherFactory.saveToBundle(initialBundle);
        assertFalse(initialBundle.containsKey(CipherFactory.BUNDLE_IV));
        assertFalse(initialBundle.containsKey(CipherFactory.BUNDLE_KEY));

        // Check that Cipher data gets saved if it exists.
        mCipherFactory.getCipher(Cipher.ENCRYPT_MODE);
        Bundle afterBundle = new Bundle();
        mCipherFactory.saveToBundle(afterBundle);
        assertTrue(afterBundle.containsKey(CipherFactory.BUNDLE_IV));
        assertTrue(afterBundle.containsKey(CipherFactory.BUNDLE_KEY));

        // Confirm the saved keys match by restoring it.
        assertTrue(mCipherFactory.restoreFromBundle(afterBundle));
    }

    /**
     * Data saved out to the {@link PersistableBundle} should match what is held by the {@link
     * CipherFactory}.
     */
    @Test
    @MediumTest
    public void testSavingToPersistableBundle() {
        // Nothing should get saved out before Cipher data exists.
        PersistableBundle initialBundle = new PersistableBundle();
        mCipherFactory.saveToPersistableBundle(initialBundle);
        assertFalse(initialBundle.containsKey(CipherFactory.PERSISTENT_BUNDLE_IV));
        assertFalse(initialBundle.containsKey(CipherFactory.PERSISTENT_BUNDLE_KEY));

        // Check that Cipher data gets saved if it exists.
        mCipherFactory.getCipher(Cipher.ENCRYPT_MODE);
        PersistableBundle afterBundle = new PersistableBundle();
        mCipherFactory.saveToPersistableBundle(afterBundle);
        assertTrue(afterBundle.containsKey(CipherFactory.PERSISTENT_BUNDLE_IV));
        assertTrue(afterBundle.containsKey(CipherFactory.PERSISTENT_BUNDLE_KEY));

        // Confirm the saved keys match by restoring it.
        assertTrue(mCipherFactory.restoreFromPersistableBundle(afterBundle));
    }

    /** Test for setting and getting the tab state storage key. */
    @Test
    @MediumTest
    @UiThreadTest
    public void testTabStateStorageKeySetAndGet() {
        // Initially, the key should be null.
        assertNull(mCipherFactory.getKeyForTabStateStorage());

        byte[] key = getRandomBytes(CipherFactory.NUM_BYTES);
        mCipherFactory.setKeyForTabStateStorage(key);
        assertTrue(Arrays.equals(key, mCipherFactory.getKeyForTabStateStorage()));

        // Attempting to set the key again should fail (assert in CipherFactory).
        // This test case would cause a CHECK failure.
        // byte[] anotherKey = getRandomBytes(CipherFactory.NUM_BYTES);
        // mCipherFactory.setKeyForTabStateStorage(anotherKey);
    }

    /** Tests saving and restoring the tab state storage key from a bundle. */
    @Test
    @MediumTest
    @UiThreadTest
    public void testTabStateStorageKeySaveAndRestore() {
        // Set a key initially.
        byte[] originalKey = getRandomBytes(CipherFactory.NUM_BYTES);
        mCipherFactory.setKeyForTabStateStorage(originalKey);

        // Save to bundle.
        Bundle bundle = new Bundle();
        mCipherFactory.saveToBundle(bundle);
        assertTrue(bundle.containsKey(CipherFactory.BUNDLE_TAB_STATE_STORAGE_KEY));
        assertTrue(
                Arrays.equals(
                        originalKey,
                        bundle.getByteArray(CipherFactory.BUNDLE_TAB_STATE_STORAGE_KEY)));

        // Create a new factory and restore from bundle.
        CipherFactory newCipherFactory = new CipherFactory();
        newCipherFactory.restoreFromBundle(bundle);
        assertEquals(originalKey, newCipherFactory.getKeyForTabStateStorage());

        // Attempt to restore with a different key when one already exists.
        byte[] differentKey = getRandomBytes(CipherFactory.NUM_BYTES);
        Bundle differentBundle = new Bundle();
        differentBundle.putByteArray(CipherFactory.BUNDLE_TAB_STATE_STORAGE_KEY, differentKey);
        newCipherFactory.restoreFromBundle(differentBundle);
        assertEquals(originalKey, newCipherFactory.getKeyForTabStateStorage());

        // Ensure legacy key and tab state storage key are independent.
        byte[] legacyKey = getRandomBytes(CipherFactory.NUM_BYTES);
        Bundle legacyBundle = new Bundle();
        legacyBundle.putByteArray(CipherFactory.BUNDLE_IV, getRandomBytes(CipherFactory.NUM_BYTES));
        legacyBundle.putByteArray(CipherFactory.BUNDLE_KEY, legacyKey);
        legacyBundle.putByteArray(CipherFactory.BUNDLE_TAB_STATE_STORAGE_KEY, differentKey);

        CipherFactory anotherCipherFactory = new CipherFactory();
        assertTrue(anotherCipherFactory.restoreFromBundle(legacyBundle));
        assertEquals(differentKey, anotherCipherFactory.getKeyForTabStateStorage());
        assertNotNull(anotherCipherFactory.getCipherData(false));
    }

    @Test
    @SmallTest
    public void testPersistBytesAsInts_multipleOf4() {
        byte[] bytes = new byte[] {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

        int[] intArray = CipherFactory.convertByteToIntArray(bytes);
        assertArrayEquals(
                new int[] {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}, intArray);
        assertArrayEquals(bytes, CipherFactory.convertIntToByteArray(intArray));
    }

    @Test
    @SmallTest
    public void testPersistBytesAsInts_notMultipleOf4() {
        byte[] bytes = new byte[] {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};

        int[] intArray = CipherFactory.convertByteToIntArray(bytes);
        assertArrayEquals(new int[] {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14}, intArray);
        assertArrayEquals(bytes, CipherFactory.convertIntToByteArray(intArray));
    }

    private PersistableBundle createPersistableBundleWithRandomKey() {
        byte[] iv = getRandomBytes(CipherFactory.NUM_BYTES);
        byte[] key = getRandomBytes(CipherFactory.NUM_BYTES);
        PersistableBundle persistableBundle = new PersistableBundle();
        persistableBundle.putIntArray(
                CipherFactory.PERSISTENT_BUNDLE_IV, CipherFactory.convertByteToIntArray(iv));
        persistableBundle.putIntArray(
                CipherFactory.PERSISTENT_BUNDLE_KEY, CipherFactory.convertByteToIntArray(key));
        return persistableBundle;
    }

    /**
     * Confirm that the two {@link Cipher}s are functionally equivalent.
     *
     * @return The input after it has been operated on (e.g. decrypted or encrypted).
     */
    private byte[] sameOutputDifferentCiphers(byte[] input, Cipher aCipher, Cipher bCipher)
            throws Exception {
        assertNotNull(aCipher);
        assertNotNull(bCipher);
        assertNotSame(aCipher, bCipher);

        byte[] aOutput = aCipher.doFinal(input);
        byte[] bOutput = bCipher.doFinal(input);

        assertNotNull(aOutput);
        assertNotNull(bOutput);
        assertTrue(Arrays.equals(aOutput, bOutput));

        return aOutput;
    }
}
