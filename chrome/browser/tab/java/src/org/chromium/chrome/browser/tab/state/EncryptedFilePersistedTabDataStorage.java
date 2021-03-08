// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.crypto.CipherFactory;

import java.util.Locale;

import javax.crypto.BadPaddingException;
import javax.crypto.Cipher;
import javax.crypto.IllegalBlockSizeException;

/**
 * Implements {@link PersistedTabDataStorage} but encrypts and decrypts
 * as data is stored and retrieved respectively.
 */
public class EncryptedFilePersistedTabDataStorage extends FilePersistedTabDataStorage {
    private static final String TAG = "EFPTDS";

    @Override
    public void save(int tabId, String dataId, Supplier<byte[]> dataSupplier) {
        save(tabId, dataId, dataSupplier, NO_OP_CALLBACK);
    }

    @Override
    @VisibleForTesting
    protected void save(
            int tabId, String dataId, Supplier<byte[]> data, Callback<Integer> callback) {
        super.save(tabId, dataId, () -> { return encrypt(data.get()); }, callback);
    }

    @Override
    public void restore(int tabId, String dataId, Callback<byte[]> callback) {
        super.restore(tabId, dataId, (data) -> { callback.onResult(decrypt(data)); });
    }

    private static byte[] decrypt(byte[] data) {
        try {
            if (data == null) {
                return null;
            }
            return CipherFactory.getInstance().getCipher(Cipher.DECRYPT_MODE).doFinal(data);
        } catch (BadPaddingException | IllegalBlockSizeException e) {
            Log.e(TAG,
                    String.format(
                            Locale.ENGLISH, "Error encrypting data. Details: %s", e.getMessage()));
            return null;
        }
    }

    private static byte[] encrypt(byte[] data) {
        Cipher cipher = CipherFactory.getInstance().getCipher(Cipher.ENCRYPT_MODE);
        try {
            return cipher.doFinal(data);
        } catch (BadPaddingException | IllegalBlockSizeException e) {
            Log.e(TAG,
                    String.format(Locale.ENGLISH, "Problem encrypting data. Details: %s",
                            e.getMessage()));
            return null;
        }
    }
}
