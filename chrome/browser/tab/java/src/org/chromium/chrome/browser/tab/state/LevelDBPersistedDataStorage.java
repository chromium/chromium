// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import androidx.annotation.MainThread;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.BrowserContextHandle;

/** Provides key -> byte[] mapping storage with namespace support for PersistedData */
public class LevelDBPersistedDataStorage implements PersistedDataStorage {
    private static boolean sSkipNativeAssertionsForTesting;
    private long mNativePersistedStateDB;
    private String mNamespace;

    /**
     * @param profile corresponding to LevelDBPersistedDataStorage instance
     *        (LevelDBPersistedDataStorage is per-profile)
     * @param namespace unique namespace which will be prepended to all keys
     */
    LevelDBPersistedDataStorage(Profile profile, String namespace) {
        assert !profile.isOffTheRecord()
                : "LevelDBPersistedTabDataStorage is not supported for incognito profiles";
        mNamespace = namespace;
        LevelDBPersistedDataStorageJni.get().init(this, profile);
        makeNativeAssertion();
    }

    @Override
    public void save(String key, byte[] data) {
        makeNativeAssertion();
        LevelDBPersistedDataStorageJni.get()
                .save(mNativePersistedStateDB, getMasterKey(key), data, null);
    }

    private String getMasterKey(String key) {
        return String.format("%s_%s", mNamespace, key);
    }

    @MainThread
    public void saveForTesting(String key, byte[] data, Runnable onComplete) {
        makeNativeAssertion();
        LevelDBPersistedDataStorageJni.get()
                .save(mNativePersistedStateDB, getMasterKey(key), data, onComplete);
    }

    @Override
    public void load(String key, Callback<byte[]> callback) {
        makeNativeAssertion();
        LevelDBPersistedDataStorageJni.get()
                .load(mNativePersistedStateDB, getMasterKey(key), callback);
    }

    @Override
    public void delete(String key) {
        makeNativeAssertion();
        LevelDBPersistedDataStorageJni.get()
                .delete(mNativePersistedStateDB, getMasterKey(key), null);
    }

    @MainThread
    public void deleteForTesting(String key, Runnable onComplete) {
        makeNativeAssertion();
        LevelDBPersistedDataStorageJni.get()
                .delete(mNativePersistedStateDB, getMasterKey(key), onComplete);
    }

    @Override
    public void performMaintenance(String[] keysToKeep, String dataId) {
        makeNativeAssertion();
        LevelDBPersistedDataStorageJni.get()
                .performMaintenance(
                        mNativePersistedStateDB, getMasterKeysToKeep(keysToKeep), dataId, null);
    }

    protected void performMaintenanceForTesting(
            String[] keysToKeep, String dataId, Runnable onComplete) {
        makeNativeAssertion();
        LevelDBPersistedDataStorageJni.get()
                .performMaintenance(
                        mNativePersistedStateDB,
                        getMasterKeysToKeep(keysToKeep),
                        dataId,
                        onComplete);
    }

    private String[] getMasterKeysToKeep(String[] keysToKeep) {
        String[] masterKeysToKeep = new String[keysToKeep.length];
        for (int i = 0; i < keysToKeep.length; i++) {
            masterKeysToKeep[i] = getMasterKey(keysToKeep[i]);
        }
        return masterKeysToKeep;
    }

    public void destroy() {
        makeNativeAssertion();
        LevelDBPersistedDataStorageJni.get().destroy(mNativePersistedStateDB);
    }

    @CalledByNative
    private void setNativePtr(long nativePtr) {
        if (!sSkipNativeAssertionsForTesting) {
            assert nativePtr != 0;
            assert mNativePersistedStateDB == 0;
        }
        mNativePersistedStateDB = nativePtr;
    }

    private void makeNativeAssertion() {
        if (!sSkipNativeAssertionsForTesting) {
            assert mNativePersistedStateDB != 0;
        }
    }

    public static void setSkipNativeAssertionsForTesting(boolean skipNativeAssertionsForTesting) {
        sSkipNativeAssertionsForTesting = skipNativeAssertionsForTesting;
        ResettersForTesting.register(() -> sSkipNativeAssertionsForTesting = false);
    }

    @NativeMethods
    public interface Natives {
        void init(LevelDBPersistedDataStorage caller, BrowserContextHandle handle);

        void destroy(long nativePersistedStateDB);

        void save(long nativePersistedStateDB, String key, byte[] data, Runnable onComplete);

        void load(long nativePersistedStateDB, String key, Callback<byte[]> callback);

        void delete(long nativePersistedStateDB, String key, Runnable onComplete);

        void performMaintenance(
                long nativePersistedStateDB,
                String[] keysToKeep,
                String dataId,
                Runnable onComplete);
    }
}
