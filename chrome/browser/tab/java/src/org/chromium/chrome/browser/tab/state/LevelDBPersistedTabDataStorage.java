// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import androidx.annotation.MainThread;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.annotations.RemovableInRelease;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.embedder_support.browser_context.BrowserContextHandle;

import java.util.Locale;

/**
 * {@link LevelDBPersistedTabDataStorage} provides a level db backed implementation
 * of {@link PersistedTabDataStorage}.
 */
public class LevelDBPersistedTabDataStorage implements PersistedTabDataStorage {
    // In a mock environment, the native code will not be running so we should not
    // make assertions about mNativePersistedStateDB
    private static boolean sSkipNativeAssertionsForTesting;
    private long mNativePersistedStateDB;
    // Callback is only used for synchronization of save and delete in testing.
    // Otherwise it is a no-op.
    // TODO(crbug.com/1146799) Apply tricks like @CheckDiscard or @RemovableInRelease to improve
    // performance
    private boolean mIsDestroyed;

    LevelDBPersistedTabDataStorage(Profile profile) {
        assert !profile.isOffTheRecord()
            : "LevelDBPersistedTabDataStorage is not supported for incognito profiles";
        LevelDBPersistedTabDataStorageJni.get().init(this, profile);
        makeNativeAssertion();
    }

    private void makeNativeAssertion() {
        if (!sSkipNativeAssertionsForTesting) {
            assert mNativePersistedStateDB != 0;
        }
    }

    @MainThread
    @Override
    public void save(int tabId, String dataId, byte[] data) {
        makeNativeAssertion();
        LevelDBPersistedTabDataStorageJni.get().save(
                mNativePersistedStateDB, getKey(tabId, dataId), data, null);
    }

    @RemovableInRelease
    @MainThread
    public void saveForTesting(int tabId, String dataId, byte[] data, Runnable onComplete) {
        makeNativeAssertion();
        LevelDBPersistedTabDataStorageJni.get().save(
                mNativePersistedStateDB, getKey(tabId, dataId), data, onComplete);
    }

    @MainThread
    @Override
    public void restore(int tabId, String dataId, Callback<byte[]> callback) {
        makeNativeAssertion();
        LevelDBPersistedTabDataStorageJni.get().load(
                mNativePersistedStateDB, getKey(tabId, dataId), callback);
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
        makeNativeAssertion();
        LevelDBPersistedTabDataStorageJni.get().delete(
                mNativePersistedStateDB, getKey(tabId, dataId), null);
    }

    @RemovableInRelease
    @MainThread
    public void deleteForTesting(int tabId, String dataId, Runnable onComplete) {
        makeNativeAssertion();
        LevelDBPersistedTabDataStorageJni.get().delete(
                mNativePersistedStateDB, getKey(tabId, dataId), onComplete);
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
        if (!sSkipNativeAssertionsForTesting) {
            assert mNativePersistedStateDB != 0;
        }
        LevelDBPersistedTabDataStorageJni.get().destroy(mNativePersistedStateDB);
        mNativePersistedStateDB = 0;
        mIsDestroyed = true;
    }

    @VisibleForTesting
    protected boolean isDestroyed() {
        return mIsDestroyed;
    }

    @CalledByNative
    private void setNativePtr(long nativePtr) {
        if (!sSkipNativeAssertionsForTesting) {
            assert nativePtr != 0;
            assert mNativePersistedStateDB == 0;
        }
        mNativePersistedStateDB = nativePtr;
    }

    @VisibleForTesting
    public static void setSkipNativeAssertionsForTesting(boolean skipNativeAssertionsForTesting) {
        sSkipNativeAssertionsForTesting = skipNativeAssertionsForTesting;
    }

    @NativeMethods
    public interface Natives {
        void init(LevelDBPersistedTabDataStorage caller, BrowserContextHandle handle);
        void destroy(long nativePersistedStateDB);
        void save(long nativePersistedStateDB, String key, byte[] data, Runnable onComplete);
        void load(long nativePersistedStateDB, String key, Callback<byte[]> callback);
        void delete(long nativePersistedStateDB, String key, Runnable onComplete);
    }
}
