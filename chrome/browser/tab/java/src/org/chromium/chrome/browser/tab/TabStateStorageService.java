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
     * <p>TODO(https://crbug.com/430996004): Scope to a given window.
     *
     * @param callback Run with loaded data.
     */
    public void loadAllData(Callback<StorageLoadedData> callback) {
        TabStateStorageServiceJni.get().loadAllData(mNativeTabStateStorageService, callback);
    }

    /** Clears all the tabs from persistent storage. */
    public void clearState() {
        TabStateStorageServiceJni.get().clearState(mNativeTabStateStorageService);
    }

    @NativeMethods
    interface Natives {
        void save(long nativeTabStateStorageServiceAndroid, @JniType("TabAndroid*") Tab tab);

        void loadAllData(
                long nativeTabStateStorageServiceAndroid,
                Callback<StorageLoadedData> loadedDataCallback);

        void clearState(long nativeTabStateStorageServiceAndroid);
    }
}
