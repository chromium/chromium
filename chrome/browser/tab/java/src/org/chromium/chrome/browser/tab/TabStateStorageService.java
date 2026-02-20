// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.tabs.TabStripCollection;

/** Java counterpart to keyed service in native that writes tab data to disk. */
@JNINamespace("tabs")
@NullMarked
public class TabStateStorageService {
    private static class ScopedStorageBatchImpl implements ScopedStorageBatch {
        private long mNativeScopedBatch;

        private ScopedStorageBatchImpl(long nativeScopedBatch) {
            mNativeScopedBatch = nativeScopedBatch;
        }

        @Override
        public void close() {
            assert mNativeScopedBatch != 0;
            if (mNativeScopedBatch != 0) {
                TabStateStorageServiceJni.get().commitBatch(mNativeScopedBatch);
                mNativeScopedBatch = 0;
            }
        }
    }

    private final long mNativeTabStateStorageService;

    private TabStateStorageService(long nativeTabStateStorageService) {
        mNativeTabStateStorageService = nativeTabStateStorageService;
    }

    @CalledByNative
    private static TabStateStorageService create(long nativeTabStateStorageService) {
        return new TabStateStorageService(nativeTabStateStorageService);
    }

    /**
     * Boosts the priority of the database operations to USER_BLOCKING until all current pending
     * operations are complete. This should be used when it is critical to save user data.
     */
    public void boostPriority() {
        TabStateStorageServiceJni.get().boostPriority(mNativeTabStateStorageService);
    }

    /**
     * Saves the tab state to persistent storage.
     *
     * @param tab The tab to save to storage.
     */
    public void saveTabData(Tab tab) {
        TabStateStorageServiceJni.get().save(mNativeTabStateStorageService, tab);
    }

    /**
     * Loads all data from persistent storage and returns it.
     *
     * <p>TODO(https://crbug.com/427254267): Add tab id/sort order to this.
     *
     * @param windowTag The window tag to load data for.
     * @param isOffTheRecord Whether to load incognito data.
     * @param callback Run with loaded data.
     */
    public void loadAllData(
            String windowTag, boolean isOffTheRecord, Callback<StorageLoadedData> callback) {
        assert !windowTag.isEmpty();
        TabStateStorageServiceJni.get()
                .loadAllData(mNativeTabStateStorageService, windowTag, isOffTheRecord, callback);
    }

    /**
     * Counts the number of tabs for a given window.
     *
     * @param windowTag The window tag to count tabs for.
     * @param isOffTheRecord Whether the tabs are off the record.
     * @param callback The callback to be called with the number of tabs.
     */
    public void countTabsForWindow(
            String windowTag, boolean isOffTheRecord, Callback<Integer> callback) {
        TabStateStorageServiceJni.get()
                .countTabsForWindow(
                        mNativeTabStateStorageService, windowTag, isOffTheRecord, callback);
    }

    /** Clears all the tabs from persistent storage. */
    public void clearState() {
        TabStateStorageServiceJni.get().clearState(mNativeTabStateStorageService);
    }

    /**
     * Clears all the tabs for a given window from persistent storage.
     *
     * @param windowTag The window tag to clear data for.
     */
    public void clearWindow(String windowTag) {
        TabStateStorageServiceJni.get().clearWindow(mNativeTabStateStorageService, windowTag);
    }

    /**
     * Clears all unused nodes for a given window from persistent storage. Any node that is not a
     * child of the given collection will be deleted.
     *
     * @param windowTag The window tag to clear unused nodes for.
     * @param isOffTheRecord Whether the nodes are off the record.
     * @param tabStripCollection The tab strip collection for a given window.
     */
    public void clearUnusedNodesForWindow(
            String windowTag,
            boolean isOffTheRecord,
            @Nullable TabStripCollection tabStripCollection) {
        if (tabStripCollection != null) {
            TabStateStorageServiceJni.get()
                    .clearUnusedNodesForWindow(
                            mNativeTabStateStorageService,
                            windowTag,
                            isOffTheRecord,
                            tabStripCollection);
        } else {
            TabStateStorageServiceJni.get()
                    .clearWindowWithOtrStatus(
                            mNativeTabStateStorageService, windowTag, isOffTheRecord);
        }
    }

    /** Clears all the tabs for a given window from persistent storage. */
    public void printAll() {
        TabStateStorageServiceJni.get().printAll(mNativeTabStateStorageService);
    }

    /** Starts a scoped batch of operations. */
    public ScopedStorageBatch createBatch() {
        long batchPtr = TabStateStorageServiceJni.get().createBatch(mNativeTabStateStorageService);
        return new ScopedStorageBatchImpl(batchPtr);
    }

    /**
     * Sets the encryption key.
     *
     * @param windowTag The window tag to set the key for.
     * @param key The encryption key.
     */
    public void setKey(String windowTag, byte[] key) {
        TabStateStorageServiceJni.get().setKey(mNativeTabStateStorageService, windowTag, key);
    }

    /**
     * Removes the encryption key.
     *
     * @param windowTag The window tag to remove the key for.
     */
    public void removeKey(String windowTag) {
        TabStateStorageServiceJni.get().removeKey(mNativeTabStateStorageService, windowTag);
    }

    /**
     * Generates a new key for encryption.
     *
     * @param windowTag The window tag to generate the key for.
     * @return The generated key.
     */
    public byte[] generateKey(String windowTag) {
        return TabStateStorageServiceJni.get()
                .generateKey(mNativeTabStateStorageService, windowTag);
    }

    @NativeMethods
    interface Natives {
        void boostPriority(long nativeTabStateStorageServiceAndroid);

        void save(long nativeTabStateStorageServiceAndroid, @JniType("TabAndroid*") Tab tab);

        void loadAllData(
                long nativeTabStateStorageServiceAndroid,
                @JniType("std::string") String windowTag,
                boolean isOffTheRecord,
                Callback<StorageLoadedData> loadedDataCallback);

        void countTabsForWindow(
                long nativeTabStateStorageServiceAndroid,
                @JniType("std::string") String windowTag,
                boolean isOffTheRecord,
                Callback<Integer> countCallback);

        void clearState(long nativeTabStateStorageServiceAndroid);

        void clearWindow(
                long nativeTabStateStorageServiceAndroid, @JniType("std::string") String windowTag);

        long createBatch(long nativeTabStateStorageServiceAndroid);

        void clearWindowWithOtrStatus(
                long nativeTabStateStorageServiceAndroid,
                @JniType("std::string") String windowTag,
                boolean isOffTheRecord);

        void clearUnusedNodesForWindow(
                long nativeTabStateStorageServiceAndroid,
                @JniType("std::string") String windowTag,
                boolean isOffTheRecord,
                @JniType("TabStripCollection*") TabStripCollection tabStripCollection);

        void printAll(long nativeTabStateStorageServiceAndroid);

        void commitBatch(long scopedBatchAndroid);

        void setKey(
                long nativeTabStateStorageServiceAndroid,
                @JniType("std::string") String windowTag,
                @JniType("std::vector<uint8_t>") byte[] key);

        void removeKey(
                long nativeTabStateStorageServiceAndroid, @JniType("std::string") String windowTag);

        byte[] generateKey(
                long nativeTabStateStorageServiceAndroid, @JniType("std::string") String windowTag);
    }
}
