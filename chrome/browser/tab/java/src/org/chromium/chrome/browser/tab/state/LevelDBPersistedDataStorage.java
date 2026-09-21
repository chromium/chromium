// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import androidx.annotation.MainThread;

import com.google.errorprone.annotations.CheckReturnValue;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.BrowserContextHandle;

/** Provides key -> byte[] mapping storage with namespace support for PersistedData */
@NullMarked
public class LevelDBPersistedDataStorage implements PersistedDataStorage {
    private static boolean sSkipNativeAssertionsForTesting;
    private long mNativePersistedStateDB;
    private final String mNamespace;

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
        if (!sSkipNativeAssertionsForTesting) {
            assert mNativePersistedStateDB != 0;
        }
    }

    @Override
    public void save(String key, byte @Nullable [] data) {
        if (!assertNativeExists()) return;
        LevelDBPersistedDataStorageJni.get()
                .save(mNativePersistedStateDB, getMasterKey(key), data, null);
    }

    private String getMasterKey(String key) {
        return String.format("%s_%s", mNamespace, key);
    }

    @MainThread
    public void saveForTesting(String key, byte[] data, @Nullable Runnable onComplete) {
        if (!assertNativeExists()) {
            if (onComplete != null) {
                PostTask.postTask(TaskTraits.UI_DEFAULT, onComplete);
            }
            return;
        }
        LevelDBPersistedDataStorageJni.get()
                .save(mNativePersistedStateDB, getMasterKey(key), data, onComplete);
    }

    @Override
    public void load(String key, Callback<byte @Nullable []> callback) {
        if (!assertNativeExists()) {
            PostTask.postTask(TaskTraits.UI_DEFAULT, callback.bind(null));
            return;
        }
        LevelDBPersistedDataStorageJni.get()
                .load(mNativePersistedStateDB, getMasterKey(key), callback);
    }

    @Override
    public void delete(String key) {
        if (!assertNativeExists()) return;
        LevelDBPersistedDataStorageJni.get()
                .delete(mNativePersistedStateDB, getMasterKey(key), null);
    }

    @MainThread
    public void deleteForTesting(String key, @Nullable Runnable onComplete) {
        if (!assertNativeExists()) {
            if (onComplete != null) {
                PostTask.postTask(TaskTraits.UI_DEFAULT, onComplete);
            }
            return;
        }
        LevelDBPersistedDataStorageJni.get()
                .delete(mNativePersistedStateDB, getMasterKey(key), onComplete);
    }

    @Override
    public void performMaintenance(String[] keysToKeep, String dataId) {
        if (!assertNativeExists()) return;
        LevelDBPersistedDataStorageJni.get()
                .performMaintenance(
                        mNativePersistedStateDB, getMasterKeysToKeep(keysToKeep), dataId, null);
    }

    protected void performMaintenanceForTesting(
            String[] keysToKeep, String dataId, @Nullable Runnable onComplete) {
        if (!assertNativeExists()) {
            if (onComplete != null) {
                PostTask.postTask(TaskTraits.UI_DEFAULT, onComplete);
            }
            return;
        }
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
        if (mNativePersistedStateDB != 0) {
            LevelDBPersistedDataStorageJni.get().destroy(mNativePersistedStateDB);
            mNativePersistedStateDB = 0;
        }
    }

    @CalledByNative
    private void setNativePtr(long nativePtr) {
        if (!sSkipNativeAssertionsForTesting) {
            assert nativePtr != 0;
            assert mNativePersistedStateDB == 0;
        }
        mNativePersistedStateDB = nativePtr;
    }

    @CheckReturnValue
    private boolean assertNativeExists() {
        if (sSkipNativeAssertionsForTesting) {
            return true;
        }
        assert mNativePersistedStateDB != 0;
        return mNativePersistedStateDB != 0;
    }

    public static void setSkipNativeAssertionsForTesting(boolean skipNativeAssertionsForTesting) {
        sSkipNativeAssertionsForTesting = skipNativeAssertionsForTesting;
        ResettersForTesting.register(() -> sSkipNativeAssertionsForTesting = false);
    }

    @NativeMethods
    public interface Natives {
        void init(LevelDBPersistedDataStorage self, BrowserContextHandle handle);

        void destroy(long nativePersistedStateDB);

        void save(
                long nativePersistedStateDB,
                @JniType("std::string") String key,
                byte @Nullable [] data,
                @Nullable Runnable onComplete);

        void load(
                long nativePersistedStateDB,
                @JniType("std::string") String key,
                Callback<byte @Nullable []> callback);

        void delete(
                long nativePersistedStateDB,
                @JniType("std::string") String key,
                @Nullable Runnable onComplete);

        void performMaintenance(
                long nativePersistedStateDB,
                @JniType("std::vector<std::string>") String[] keysToKeep,
                @JniType("std::string") String dataId,
                @Nullable Runnable onComplete);
    }
}
