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

/** Java counterpart to keyed service in native that writes tab data to disk. */
@JNINamespace("tabs")
@NullMarked
public class TabStateStorageService {

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

    /** Clears all the tabs from persistent storage. */
    public void clearState() {
        TabStateStorageServiceJni.get().clearState(mNativeTabStateStorageService);
    }

    /** Clears all the tabs for a given window from persistent storage. */
    public void clearWindow(String windowTag) {
        TabStateStorageServiceJni.get().clearWindow(mNativeTabStateStorageService, windowTag);
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

        void clearState(long nativeTabStateStorageServiceAndroid);

        void clearWindow(
                long nativeTabStateStorageServiceAndroid, @JniType("std::string") String windowTag);
    }
}
