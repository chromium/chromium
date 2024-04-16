// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import androidx.annotation.MainThread;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.chrome.browser.profiles.Profile;

import java.nio.ByteBuffer;
import java.util.List;
import java.util.Locale;

/**
 * {@link LevelDBPersistedTabDataStorage} provides a level db backed implementation
 * of {@link PersistedTabDataStorage}.
 */
public class LevelDBPersistedTabDataStorage implements PersistedTabDataStorage, Destroyable {
    // In a mock environment, the native code will not be running so we should not
    // make assertions about mNativePersistedStateDB
    // LevelDBPersistedTabDataStorage needs to have an empty namespace for backwards compatibility.
    // LevelDBPersitsedDataStorage is a generalization of the original
    // LevelDBPersistedTabDataStorage which introduced namespaces to avoid collisions between
    // clients.
    private static String sNamespace = "";
    private LevelDBPersistedDataStorage mPersistedDataStorage;
    // Callback is only used for synchronization of save and delete in testing.
    // Otherwise it is a no-op.
    // TODO(crbug.com/40156389) Apply tricks like @CheckDiscard or proguard rules to improve
    // performance
    private boolean mIsDestroyed;

    LevelDBPersistedTabDataStorage(Profile profile) {
        assert !profile.isOffTheRecord()
                : "LevelDBPersistedTabDataStorage is not supported for incognito profiles";
        mPersistedDataStorage = new LevelDBPersistedDataStorage(profile, sNamespace);
    }

    @MainThread
    @Override
    public void save(int tabId, String dataId, Serializer<ByteBuffer> serializer) {
        // TODO(crbug.com/40186903) update LevelDB storage in native to use ByteBuffer instead
        // of byte[] to avoid conversion
        serializer.preSerialize();
        mPersistedDataStorage.save(getKey(tabId, dataId), toByteArray(serializer.get()));
    }

    @Override
    public void save(
            int tabId,
            String dataId,
            Serializer<ByteBuffer> serializer,
            Callback<Integer> callback) {
        assert false : "save with callback unused in LevelDBPersistedTabDataStorage";
    }

    private static byte[] toByteArray(ByteBuffer buffer) {
        if (buffer == null) {
            return null;
        }
        if (buffer.hasArray() && buffer.arrayOffset() == 0) {
            return buffer.array();
        }
        byte[] bytes = new byte[buffer.limit()];
        buffer.rewind();
        buffer.get(bytes);
        return bytes;
    }

    @MainThread
    public void saveForTesting(int tabId, String dataId, byte[] data, Runnable onComplete) {
        mPersistedDataStorage.saveForTesting(getKey(tabId, dataId), data, onComplete); // IN-TEST
    }

    @MainThread
    @Override
    public void restore(int tabId, String dataId, Callback<ByteBuffer> callback) {
        mPersistedDataStorage.load(
                getKey(tabId, dataId),
                (res) -> {
                    callback.onResult(res == null ? null : ByteBuffer.wrap(res));
                });
    }

    /**
     * Synchronous restore was an exception provided for an edge case in
     * {@link CriticalPersistedTabData} and is not typically part of the public API.
     */
    @Deprecated
    @MainThread
    @Override
    public ByteBuffer restore(int tabId, String dataId) {
        assert false : "Synchronous restore is not supported for LevelDBPersistedTabDataStorage";
        return null;
    }

    @MainThread
    @Override
    public <U extends PersistedTabDataResult> U restore(
            int tabId, String dataId, PersistedTabDataMapper<U> mapper) {
        assert false : "Restore with mapper currently unused in LevelDBPersistedTabDataStorage";
        return null;
    }

    @MainThread
    @Override
    public <U extends PersistedTabDataResult> void restore(
            int tabId, String dataId, Callback<U> callback, PersistedTabDataMapper<U> mapper) {
        assert false : "Restore with mapper currently unused in LevelDBPersistedTabDataStorage";
    }

    @MainThread
    @Override
    public void delete(int tabId, String dataId) {
        mPersistedDataStorage.delete(getKey(tabId, dataId));
    }

    @MainThread
    public void deleteForTesting(int tabId, String dataId, Runnable onComplete) {
        mPersistedDataStorage.deleteForTesting(getKey(tabId, dataId), onComplete); // IN-TEST
    }

    @Override
    public String getUmaTag() {
        return "LevelDB";
    }

    @Override
    public void performMaintenance(List<Integer> tabIds, String dataId) {
        mPersistedDataStorage.performMaintenance(getKeysToKeep(tabIds, dataId), dataId);
    }

    public void performMaintenanceForTesting(
            List<Integer> tabIds, String dataId, Runnable onComplete) {
        mPersistedDataStorage.performMaintenanceForTesting(
                getKeysToKeep(tabIds, dataId), dataId, onComplete); // IN-TEST
    }

    private static String[] getKeysToKeep(List<Integer> tabIds, String dataId) {
        String[] keysToKeep = new String[tabIds.size()];
        for (int i = 0; i < tabIds.size(); i++) {
            keysToKeep[i] = getKey(tabIds.get(i), dataId);
        }
        return keysToKeep;
    }

    // TODO(crbug.com/40156023) Implement URL -> byte[] mapping rather
    // than tab id -> byte[] mapping so we don't store the same data
    // multiple times when the user has multiple tabs at the same URL.
    private static final String getKey(int tabId, String dataId) {
        return String.format(Locale.US, "%d-%s", tabId, dataId);
    }

    /** Destroy native instance of persisted_tab_state */
    @Override
    public void destroy() {
        mPersistedDataStorage.destroy();
        mIsDestroyed = true;
    }

    @VisibleForTesting
    protected boolean isDestroyed() {
        return mIsDestroyed;
    }
}
