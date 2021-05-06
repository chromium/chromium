// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import android.os.SystemClock;

import androidx.annotation.MainThread;
import androidx.annotation.VisibleForTesting;
import androidx.core.util.AtomicFile;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.StreamUtil;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.crypto.CipherFactory;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.Locale;

import javax.crypto.Cipher;
import javax.crypto.CipherInputStream;
import javax.crypto.CipherOutputStream;

/**
 * Implements {@link PersistedTabDataStorage} but encrypts and decrypts
 * as data is stored and retrieved respectively.
 */
public class EncryptedFilePersistedTabDataStorage extends FilePersistedTabDataStorage {
    private static final String TAG = "EFPTDS";
    // As described in {@link CipherFactory} not all incognito Tabs are restored - only
    // if the Cipher parameters are saved (which occurs when the Activity is killed in
    // the background and doesn't occur if the user explicitly closes the app).
    // In the event that the cipher parameters were saved, KEY_CHECKER will be as expected
    // when it is read. Otherwise we don't attempt to read the encrypted file (since we
    // don't have the cipher parameters).
    // 0 (as a long) is a reasonable value to use here. The likelihood of an encrypted 0L
    // for one set of cipher parameters being decrypted as 0L for another set of cipher
    // parameters is very low (1 / (2 ^ 63 - 1)).
    private static final long KEY_CHECKER = 0;

    @MainThread
    @Override
    public void save(int tabId, String dataId, Supplier<byte[]> dataSupplier) {
        save(tabId, dataId, dataSupplier, NO_OP_CALLBACK);
    }

    @MainThread
    @Override
    @VisibleForTesting
    protected void save(
            int tabId, String dataId, Supplier<byte[]> data, Callback<Integer> callback) {
        addStorageRequestAndProcessNext(
                new EncryptedFileSaveRequest(tabId, dataId, data, callback));
    }

    @MainThread
    @Override
    public void restore(int tabId, String dataId, Callback<byte[]> callback) {
        addStorageRequestAndProcessNext(new EncryptedFileRestoreRequest(tabId, dataId, callback));
    }

    @MainThread
    @Override
    public byte[] restore(int tabId, String dataId) {
        return new EncryptedFileRestoreRequest(tabId, dataId, null).executeSyncTask();
    }

    /**
     * Request to save encrypted file based {@link PersistedTabData}
     */
    private class EncryptedFileSaveRequest extends FileSaveRequest {
        /**
         * @param tabId identifier for the {@link Tab}
         * @param dataId identifier for the {@link PersistedTabData}
         * @param dataSupplier {@link Supplier} containing data to be saved
         */
        EncryptedFileSaveRequest(int tabId, String dataId, Supplier<byte[]> dataSupplier,
                Callback<Integer> callback) {
            super(tabId, dataId, dataSupplier, callback);
        }

        @Override
        public Void executeSyncTask() {
            byte[] data = mDataSupplier.get();
            if (data == null) {
                mDataSupplier = null;
                return null;
            }
            boolean success = false;
            Cipher cipher = CipherFactory.getInstance().getCipher(Cipher.ENCRYPT_MODE);
            if (cipher == null) {
                Log.e(TAG, "Cipher is null so cannot save encrypted file based PersistedTabData");
                return null;
            }
            FileOutputStream fileOutputStream = null;
            CipherOutputStream cipherOutputStream = null;
            DataOutputStream dataOutputStream = null;
            AtomicFile atomicFile = null;
            try {
                long startTime = SystemClock.elapsedRealtime();
                atomicFile = new AtomicFile(mFile);
                fileOutputStream = atomicFile.startWrite();
                cipherOutputStream = new CipherOutputStream(fileOutputStream, cipher);
                dataOutputStream = new DataOutputStream(cipherOutputStream);
                dataOutputStream.writeLong(KEY_CHECKER);
                dataOutputStream.writeInt(data.length);
                dataOutputStream.write(data);
                success = true;
                RecordHistogram.recordTimesHistogram(
                        String.format(Locale.US, "Tabs.PersistedTabData.Storage.SaveTime.%s",
                                getUmaTag()),
                        SystemClock.elapsedRealtime() - startTime);
            } catch (FileNotFoundException e) {
                Log.e(TAG,
                        String.format(Locale.ENGLISH,
                                "FileNotFoundException while attempting to save file %s "
                                        + "Details: %s",
                                mFile, e.getMessage()));
                return null;
            } catch (IOException e) {
                Log.e(TAG,
                        String.format(Locale.ENGLISH,
                                "IOException while attempting to save file %s. "
                                        + " Details: %s",
                                mFile, e.getMessage()));
            } finally {
                StreamUtil.closeQuietly(dataOutputStream);
                StreamUtil.closeQuietly(cipherOutputStream);
                StreamUtil.closeQuietly(fileOutputStream);
                if (atomicFile != null) {
                    if (success) {
                        atomicFile.finishWrite(fileOutputStream);
                    } else {
                        atomicFile.failWrite(fileOutputStream);
                    }
                }
            }
            RecordHistogram.recordBooleanHistogram(
                    "Tabs.PersistedTabData.Storage.Save." + getUmaTag(), success);
            return null;
        }
    }

    /**
     * Request to restore an encrypted {@link PersistedTabData}
     */
    private class EncryptedFileRestoreRequest extends FileRestoreRequest {
        /**
         * @param tabId identifier for the {@link Tab}
         * @param dataId identifier for the {@link PersistedTabData}
         * @param callback - callback to return the retrieved serialized
         * {@link PersistedTabData} in
         */
        EncryptedFileRestoreRequest(int tabId, String dataId, Callback<byte[]> callback) {
            super(tabId, dataId, callback);
        }

        @Override
        public byte[] executeSyncTask() {
            Cipher cipher = CipherFactory.getInstance().getCipher(Cipher.DECRYPT_MODE);
            if (cipher == null) {
                Log.e(TAG,
                        "Cipher is null so cannot restore encrypted file based PersistedTabData");
                return null;
            }
            boolean success = false;
            byte[] res = null;
            FileInputStream fileInputStream = null;
            CipherInputStream cipherInputStream = null;
            DataInputStream dataInputStream = null;
            long startTime = SystemClock.elapsedRealtime();
            try {
                AtomicFile atomicFile = new AtomicFile(mFile);
                fileInputStream = atomicFile.openRead();
                cipherInputStream = new CipherInputStream(fileInputStream, cipher);
                dataInputStream = new DataInputStream(cipherInputStream);
                if (dataInputStream.readLong() != KEY_CHECKER) {
                    // Cipher parameters were not saved if KEY_CHECKER is not as expected.
                    // No need to attempt reading/decrypting the byte array, since we know
                    // it will fail.
                    return null;
                }
                res = new byte[dataInputStream.readInt()];
                dataInputStream.readFully(res);
                success = true;
            } catch (FileNotFoundException e) {
                Log.e(TAG,
                        String.format(Locale.ENGLISH,
                                "FileNotFoundException while attempting to restore "
                                        + " %s. Details: %s",
                                mFile, e.getMessage()));
            } catch (IOException e) {
                Log.e(TAG,
                        String.format(Locale.ENGLISH,
                                "IOException while attempting to restore "
                                        + "%s. Details: %s",
                                mFile, e.getMessage()));
            } finally {
                StreamUtil.closeQuietly(dataInputStream);
                StreamUtil.closeQuietly(cipherInputStream);
                StreamUtil.closeQuietly(fileInputStream);
                // TODO(crbug.com/1204889) Create separate histograms for encrypted file based
                // {@link PersistedTabData}
                RecordHistogram.recordTimesHistogram(
                        String.format(Locale.US, "Tabs.PersistedTabData.Storage.LoadTime.%s",
                                getUmaTag()),
                        SystemClock.elapsedRealtime() - startTime);
            }
            RecordHistogram.recordBooleanHistogram(
                    "Tabs.PersistedTabData.Storage.Restore." + getUmaTag(), success);
            return res;
        }
    }
}
