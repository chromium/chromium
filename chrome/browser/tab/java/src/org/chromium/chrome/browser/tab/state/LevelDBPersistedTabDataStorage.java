// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import androidx.annotation.MainThread;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.Locale;

/**
 * {@link LevelDBPersistedTabDataStorage} provides a level db backed implementation
 * of {@link PersistedTabDataStorage}.
 */
public class LevelDBPersistedTabDataStorage implements PersistedTabDataStorage {
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
    // TODO(crbug.com/1146799) Apply tricks like @CheckDiscard or @RemovableInRelease to improve
    // performance
    private boolean mIsDestroyed;

    LevelDBPersistedTabDataStorage(Profile profile) {
        assert !profile.isOffTheRecord()
            : "LevelDBPersistedTabDataStorage is not supported for incognito profiles";
        mPersistedDataStorage = new LevelDBPersistedDataStorage(profile, sNamespace);
    }

    @MainThread
    @Override
    public void save(int tabId, String dataId, Supplier<byte[]> dataSupplier) {
        mPersistedDataStorage.save(getKey(tabId, dataId), dataSupplier.get());
    }

    @MainThread
    public void saveForTesting(int tabId, String dataId, byte[] data, Runnable onComplete) {
        mPersistedDataStorage.saveForTesting(getKey(tabId, dataId), data, onComplete); // IN-TEST
    }

    @MainThread
    @Override
    public void restore(int tabId, String dataId, Callback<byte[]> callback) {
        mPersistedDataStorage.load(getKey(tabId, dataId), callback);
    }

    /**
     * Synchronous restore was an exception provided for an edge case in
     * {@link CriticalPersistedTabData} and is not typically part of the public API.
     */
    @Deprecated
    @MainThread
    @Override
    public byte[] restore(int tabId, String dataId) {
        assert false : "Synchronous restore is not supported for LevelDBPersistedTabDataStorage";
        return null;
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

    // TODO(crbug.com/1145785) Implement URL -> byte[] mapping rather
    // than tab id -> byte[] mapping so we don't store the same data
    // multiple times when the user has multiple tabs at the same URL.
    private static final String getKey(int tabId, String dataId) {
        return String.format(Locale.US, "%d-%s", tabId, dataId);
    }

    /**
     * Destroy native instance of persisted_tab_state
     */
    public void destroy() {
        mPersistedDataStorage.destroy();
        mIsDestroyed = true;
    }

    @VisibleForTesting
    protected boolean isDestroyed() {
        return mIsDestroyed;
    }

}
